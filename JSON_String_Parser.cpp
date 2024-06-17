#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <variant>


using namespace std;

struct JsonObject;
struct JsonArray;

using JsonValue = variant<JsonObject, JsonArray, string, int, double, bool, nullptr_t>;

struct JsonObject : public map<string, JsonValue>
{
};
struct JsonArray : public vector<JsonValue>
{
};

enum class TokenType
{
    STRING,
    NUMBER,
    BOOLEAN,
    NULL_VALUE,
    COMMA,
    COLON,
    LEFT_CURLY,
    RIGHT_CURLY,
    LEFT_SQUARE,
    RIGHT_SQUARE,
    EOF_TOKEN
};

class Token
{
public:
    TokenType token_type;
    variant<monostate, string, int, double, bool> value;
    Token(TokenType token_type, variant<monostate, string, int, double, bool> value) : token_type(token_type), value(value) {}
};

class Scanner
{
private:
    string str;
    size_t start;
    size_t current;
    size_t line;
    vector<Token> tokens;

public:
    Scanner(const string &str) : str(str), start(0), current(0), line(1) {}

    vector<Token> scan()
    {
        while (!isLast())
        {
            start = current;
            scanToken();
        }

        tokens.push_back(Token{TokenType::EOF_TOKEN, monostate{}});
        return tokens;
    }

private:
    bool isLast() const
    {
        return current >= str.length();
    }

    void scanToken()
    {
        char c = advanceCurrent();
        switch (c)
        {
        case '{':
            tokens.push_back(Token{TokenType::LEFT_CURLY, string(1, c)});
            break;
        case '}':
            tokens.push_back(Token{TokenType::RIGHT_CURLY, string(1, c)});
            break;
        case '[':
            tokens.push_back(Token{TokenType::LEFT_SQUARE, string(1, c)});
            break;
        case ']':
            tokens.push_back(Token{TokenType::RIGHT_SQUARE, string(1, c)});
            break;
        case ',':
            tokens.push_back(Token{TokenType::COMMA, string(1, c)});
            break;
        case ':':
            tokens.push_back(Token{TokenType::COLON, string(1, c)});
            break;
        case '\n':
            line++;
            break;
        case ' ':
            break;
        case '"':
            addString();
            break;
        case '-':
            if (isdigit(getCurrValue()))
            {
                advanceCurrent();
                addNumber();
            }
            else
            {
                throw invalid_argument("- must be followed by a number");
            }
            break;
        default:
            if (isdigit(c))
            {
                addNumber();
            }
            else if (isalpha(c))
            {
                addKeyword();
            }
            else
            {
                throw invalid_argument("Unexpected token");
            }
        }
    }

     void addKeyword()
    {
        while (isalpha(getCurrValue()))
        {
            current++;
        }

        string keyword = str.substr(start, current - start);
        if (keyword == "true")
        {
            tokens.push_back(Token{TokenType::BOOLEAN, true});
        }
        else if (keyword == "false")
        {
            tokens.push_back(Token{TokenType::BOOLEAN, false});
        }
        else if (keyword == "null")
        {
            tokens.push_back(Token{TokenType::NULL_VALUE, monostate{}});
        }
        else
        {
            throw invalid_argument("Unexpected token");
        }
    }

    
    void addNumber()
    {
        while (isdigit(getCurrValue()))
        {
            current++;
        }

        if (getCurrValue() == '.')
        {
            if (!isdigit(getNextValue()))
            {
                throw invalid_argument("There should be digit after (.) ");
            }

            current++;

            while (isdigit(getCurrValue()))
            {
                current++;
            }

            tokens.push_back(Token{TokenType::NUMBER, stod(str.substr(start, current - start))});
        }
        else
        {
            tokens.push_back(Token{TokenType::NUMBER, stoi(str.substr(start, current - start))});
        }
    }

    void addString()
    {
        while (getCurrValue() != '"' && !isLast())
        {
            current++;
        }

        if (isLast())
        {
            throw invalid_argument("String has not been terminated");
        }

        current++;

        tokens.push_back(Token{TokenType::STRING, str.substr(start + 1, current - start - 2)});
    }

    char getNextValue()
    {
        if (current + 1 >= str.length())
        {
            return ' ';
        }

        return str[current + 1];
    }
    char getCurrValue()
    {
        if (isLast())
        {
            return ' ';
        }
        return str[current];
    }
    
    char advanceCurrent()
    {
        char c = str[current++];
        return c;
    }
    
};

class Parser
{
private:
    vector<Token> tokens;
    size_t current;

public:
    Parser(const vector<Token> &tokens) : tokens(tokens), current(0) {}

    JsonValue parse()
    {
        Token token = advanceCurrent();
        return parse_from_token(token);
    }

    JsonValue parse_from_token(const Token &token)
    {
        switch (token.token_type)
        {
        case TokenType::STRING:
            return get<string>(token.value);
        case TokenType::NUMBER:
            if (holds_alternative<int>(token.value))
            {
                return get<int>(token.value);
            }
            else
            {
                return get<double>(token.value);
            }
        case TokenType::BOOLEAN:
            return get<bool>(token.value);
        case TokenType::NULL_VALUE:
            return nullptr;
        case TokenType::LEFT_CURLY:
            return parse_object();
        case TokenType::LEFT_SQUARE:
            return parse_array();
        default:
            throw invalid_argument("Unexpected token");
        }
    }
    
    
    Token advanceCurrent()
    {
        Token token = tokens[current];
        current++;
        return token;
    }
    
     void consume(TokenType token_type, const string &error)
    {
        if (getCurrValue().token_type != token_type)
        {
            throw invalid_argument(error);
        }

        current++;
    }

    void consume_until_comma(TokenType exception)
    {
        if (getCurrValue().token_type == TokenType::COMMA)
        {
            current++;
            return;
        }

        if (getCurrValue().token_type != exception)
        {
            throw invalid_argument("Missing comma unless followed by");
        }
    }
    
    JsonArray parse_array()
    {
        JsonArray json_array;

        Token token = advanceCurrent();
        while (token.token_type != TokenType::RIGHT_SQUARE)
        {
            if (token.token_type == TokenType::EOF_TOKEN)
            {
                throw invalid_argument("Unterminated JSON array");
            }

            json_array.push_back(parse_from_token(token));

            consume_until_comma(TokenType::RIGHT_SQUARE);
            token = advanceCurrent();
        }

        return json_array;
    }
    
    JsonObject parse_object()
    {
        JsonObject json_object;

        Token key_token = advanceCurrent();
        while (key_token.token_type != TokenType::RIGHT_CURLY)
        {
            if (key_token.token_type == TokenType::EOF_TOKEN)
            {
                throw invalid_argument("Unterminated JSON object");
            }

            if (key_token.token_type != TokenType::STRING)
            {
                throw invalid_argument("JSON object fields must begin with a 'key' i.e. string");
            }

            consume(TokenType::COLON, "Key: values must be colon separated");

            Token value_token = advanceCurrent();
            json_object[get<string>(key_token.value)] = parse_from_token(value_token);

            consume_until_comma(TokenType::RIGHT_CURLY);
            key_token = advanceCurrent();
        }

        return json_object;
    }


    Token getCurrValue()
    {
        return tokens[current];
    }
};

JsonValue parse(const string &input)
{
    Scanner scanner(input);
    auto tokens = scanner.scan();
    Parser parser(tokens);
    return parser.parse();
}

void print_json(const JsonValue &value, int indent = 0);

void print_json_array(const JsonArray &arr, int indent)
{
    cout << string(indent, ' ') << "[\n";
    for (size_t i = 0; i < arr.size(); ++i)
    {
        print_json(arr[i], indent + 2);
        if (i < arr.size() - 1)
            cout << ",";
        cout << "\n";
    }
    cout << string(indent, ' ') << "]";
}

void print_json_object(const JsonObject &obj, int indent)
{
    cout << string(indent, ' ') << "{\n";
    for (auto it = obj.begin(); it != obj.end(); ++it)
    {
        cout << string(indent + 2, ' ') << "\"" << it->first << "\": ";
        print_json(it->second, indent + 2);
        if (next(it) != obj.end())
            cout << ",";
        cout << "\n";
    }
    cout << string(indent, ' ') << "}";
}

void print_json(const JsonValue &value, int indent)
{
    if (holds_alternative<JsonObject>(value))
    {
        print_json_object(get<JsonObject>(value), indent);
    }
    else if (holds_alternative<JsonArray>(value))
    {
        print_json_array(get<JsonArray>(value), indent);
    }
    else if (holds_alternative<string>(value))
    {
        cout << "\"" << get<string>(value) << "\"";
    }
    else if (holds_alternative<int>(value))
    {
        cout << get<int>(value);
    }
    else if (holds_alternative<double>(value))
    {
        cout << get<double>(value);
    }
    else if (holds_alternative<bool>(value))
    {
        cout << (get<bool>(value) ? "true" : "false");
    }
    else if (holds_alternative<nullptr_t>(value))
    {
        cout << "null";
    }
}




int main()
{
    cout << "--------------------------Example 1-----------------------" << endl;
    string input1 = R"({"employees":[{"firstName":"Rashmi","lastName":"Thakur"},{"firstName":"Riya","lastName":"Thakur"},{"firstName":"Rohan","lastName":"Thakur"}]})";
    JsonValue result1 = parse(input1);
    print_json(result1);

    cout << "--------------------------Example 2-----------------------" << endl;
    string input2 = R"({"name":"Rashmi", "age":21, "car":null})";
    JsonValue result2 = parse(input2);
    print_json(result2);

    cout << "--------------------------Example 3-----------------------" << endl;
    string input3 = R"({
"company": {
    "name": "Deloitte India",
    "address": {
      "street": "Stree-1",
      "city": "Gurugram",
      "state": "India",
      "zipcode": 122002
    },
    "employees": [
      {
        "id": 1,
        "name": "Richa Garg",
        "position": " Data Analyst",
        "salary": 100000
      },
      {
        "id": 2,
        "name": "Sohan Lal",
        "position": "Engineer",
        "salary": 120000
      },
      {
        "id": 3,
        "name": "Parul Mehta",
        "position": "Manager",
        "salary": 200000
      }
    ],
    "departments": {
      "engineering": ["Richa Garg", "Sohan Lal"],
      "management": ["Parul Mehta"]
    }
  },
  "projects": [
    {
      "id": "project-01",
      "name": "New Scheme Launch Website",
      "description": "Development of a new website for new scheme launch",
      "status": "completed",
      "team": ["Sohan Lal", "Richa Garg"],
      "budget": 30000
    },
    {
      "id": "project-02",
      "name": "Data Analytics Platform",
      "description": "Building a data analytics platform for business insights",
      "status": "in_progress",
      "team": ["Richa Garg"],
      "budget": 55000
    },
    {
      "id": "project-03",
      "name": "Product Launch",
      "description": "Launching a new product into the market",
      "status": "Planned and Designed",
      "team": ["Parul Mehta"],
      "budget": 200000
    }
  ]
}
)";
    JsonValue result3 = parse(input3);
    print_json(result3);

    return 0;
}
