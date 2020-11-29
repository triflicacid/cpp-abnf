#include "../include/variable.hpp"

#include <regex>
#include <string>

#include "../include/utils.hpp"

namespace abnf {
Variable::Variable(const char* name, std::string decl_str, int start_def) {
    _name = name;
    def_string = decl_str;
    _start_def = start_def;
}

std::string Variable::get_name() { return _name; }

/**
 *  String: { "string", { flag, str } }
 *  Numeric { "numeric", { base, num, ... } }
 *  Numeric Range: { "numeric_range", { base, lower, upper } }
 */
int Variable::lex(std::string& msg) {
    std::vector<token_t> tokens;

    // Data for next token
    std::string token_type;
    std::vector<std::string> token_data;
    int t_start;  // Start position

    int lim = def_string.length();
    std::string source = def_string;  // Copy of def_string
    int inc = 1;                      // How much to increment pos by?

    for (int pos = 0; pos < lim;) {
        char c = source[0];
        inc = 1;

        // Scan for leading/opening symbols...
        if (token_type == "") {
            if (c == ' ')
                ;  // Skip whitespace
            else if (c == '"') {
                // STRING
                token_type = "string";
                if (token_data.size() == 0)
                    // Default flag: case insentitive
                    token_data.push_back("i");
                token_data.push_back("");
            } else if (c == '%') {
                // NUMERIC CHARACTER
                token_type = "percent";
            } else if (c == ';') {
                // Comment
                break;
            } else {
                msg = "error: unknown identification token " + ctos(c);
                return pos;
            }
            t_start = pos;  // Set starting position
        } else if (token_type == "string") {
            if (c == '\\' && source.length() > 1) {
                // Escape char
                token_data[1] += c + source[1];
                inc = 2;
            } else if (c == '"') {
                // Terminate string
                _push_token(token_type, token_data, t_start, tokens);
            } else {
                // Add to string literal
                token_data[1] += c;
            }
        } else if (token_type == "percent") {
            if (source.length() == 1) {
                msg = "error: unexpected end of input";
                return false;
            } else if (c == 'b' || c == 'd' || c == 'x') {
                char d = source[1];
                if (digit_match_base(c, d)) {
                    // Numeric Base
                    token_type = "numeric";
                    token_data.push_back(ctos(c));
                    token_data.push_back(ctos(d));
                    inc = 2;
                } else {
                    msg = "error: invalid literal for base " + ctos(c) + ": " +
                          ctos(d);
                    return pos + 1;
                }
            } else if (c == 's' || c == 'i') {
                // String flag
                if (source[1] == '"') {
                    token_type = "";
                    token_data.push_back(ctos(c));
                    // -- Carried forward to "string"
                } else {
                    msg =
                        "error: expected string to follow string flag: "
                        "expected \", got " +
                        ctos(source[1]);
                    return pos + 1;
                }
            } else {
                msg = "error: invalid token succeeding %: " + ctos(c);
                return pos;
            }
        } else if (token_type == "numeric") {
            // Digit?
            char base = token_data[0][0];
            if (digit_match_base(base, c)) {
                *get_last(token_data) += c;
            } else if (c == '.') {
                // Concatenate (expect digit)
                if (get_last(token_data)->length() == 0) {
                    msg = "error: invalid base-" + ctos(base) +
                          " digit: " + ctos(c);
                    return pos;
                    return pos;
                } else {
                    // Start new numeric literal
                    token_data.push_back("");
                }
            } else if (c == '-') {
                // Range
                token_data.push_back("");
                token_type = "numeric_range";
            } else {
                _push_token(token_type, token_data, t_start, tokens);
                inc = 0;
            }
        } else if (token_type == "numeric_range") {
            char base = token_data[0][0];
            if (digit_match_base(base, c)) {
                *get_last(token_data) += c;
            } else {
                // There must be something here
                if (get_last(token_data)->length() == 0) {
                    msg = "error: invalid base-" + ctos(base) +
                          " digit: " + ctos(c);
                    return pos;
                } else {
                    // Check that range is positive
                    int base = charbase_to_int(token_data[0][0]);
                    int lower = std::stoi(token_data[1], nullptr, base);
                    int upper = std::stoi(token_data[2], nullptr, base);
                    printf("Range: %s - %s \n", std::to_string(lower).c_str(),
                           std::to_string(upper).c_str());
                    if (lower >= upper) {
                        msg =
                            "error: numeric range: upper bound must be greater "
                            "than lower bound";
                        return pos - token_data[2].length();
                    }

                    _push_token(token_type, token_data, t_start, tokens);
                    inc = 0;
                }
            }
        } else {
            msg = "error: unknown token type " + token_type;
            return pos;
        }

        // Increment
        if (inc > 0) {
            pos += inc;
            source.erase(0, inc);
        }
    }

    // Anything left over?
    if (token_type == "string" ||
        (token_type == "numeric" && *get_last(token_data) == "") ||
        (token_type == "numeric_range" &&
         (token_data.size() != 3 || *get_last(token_data) == ""))) {
        // If string, or unfinished concatenation, or unfinished range...
        msg = "error: unexpected EOL whilst scanning " + token_type;
        return lim;
    } else if (token_type != "")
        // Push trailing token
        _push_token(token_type, token_data, t_start, tokens);

    _tokens = tokens;
    return -1;
}  // namespace abnf

void Variable::lex_fatal() {
    std::string msg;
    int fpos = lex(msg);
    if (fpos > -1) {
        throw_error(def_string, msg, fpos, fpos + 1);
    }
}

// [DEBUG] Print tokens to screen
void Variable::print_tokens() {
    printf("Tokens: \n");
    for (auto token : _tokens) {
        printf("- %s: ", std::get<0>(token).c_str());
        for (auto t : std::get<1>(token)) printf("`%s` ", t.c_str());
        printf(" (position %d)\n", std::get<2>(token));
    }
}
};  // namespace abnf