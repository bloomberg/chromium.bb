// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_parser.h"

#include <cmath>
#include <utility>
#include <vector>

#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "base/third_party/icu/icu_utf.h"
#include "base/values.h"

namespace base {
namespace internal {

namespace {

const int32_t kExtendedASCIIStart = 0x80;

// Simple class that checks for maximum recursion/"stack overflow."
class StackMarker {
 public:
  StackMarker(int max_depth, int* depth)
      : max_depth_(max_depth), depth_(depth) {
    ++(*depth_);
    DCHECK_LE(*depth_, max_depth_);
  }
  ~StackMarker() {
    --(*depth_);
  }

  bool IsTooDeep() const { return *depth_ >= max_depth_; }

 private:
  const int max_depth_;
  int* const depth_;

  DISALLOW_COPY_AND_ASSIGN(StackMarker);
};

constexpr uint32_t kUnicodeReplacementPoint = 0xFFFD;

}  // namespace

// This is U+FFFD.
const char kUnicodeReplacementString[] = "\xEF\xBF\xBD";

JSONParser::JSONParser(int options, int max_depth)
    : options_(options),
      max_depth_(max_depth),
      index_(0),
      stack_depth_(0),
      line_number_(0),
      index_last_line_(0),
      error_code_(JSONReader::JSON_NO_ERROR),
      error_line_(0),
      error_column_(0) {
  CHECK_LE(max_depth, JSONReader::kStackMaxDepth);
}

JSONParser::~JSONParser() = default;

Optional<Value> JSONParser::Parse(StringPiece input) {
  input_ = input;
  index_ = 0;
  line_number_ = 1;
  index_last_line_ = 0;

  error_code_ = JSONReader::JSON_NO_ERROR;
  error_line_ = 0;
  error_column_ = 0;

  // ICU and ReadUnicodeCharacter() use int32_t for lengths, so ensure
  // that the index_ will not overflow when parsing.
  if (!base::IsValueInRangeForNumericType<int32_t>(input.length())) {
    ReportError(JSONReader::JSON_TOO_LARGE, 0);
    return nullopt;
  }

  // When the input JSON string starts with a UTF-8 Byte-Order-Mark
  // <0xEF 0xBB 0xBF>, advance the start position to avoid the
  // ParseNextToken function mis-treating a Unicode BOM as an invalid
  // character and returning NULL.
  if (ConsumeIfMatch("\xEF\xBB\xBF")) {
    NextChar();  // Advance past the last byte of the BOM.
  }

  // Parse the first and any nested tokens.
  Optional<Value> root(ParseNextToken());
  if (!root)
    return nullopt;

  // Make sure the input stream is at an end.
  if (GetNextToken() != T_END_OF_INPUT) {
    // There may be trailing whitespace or comments, so advance the parser
    // and consume that via GetNextToken().
    if (CanConsume(1)) {
      NextChar();
    }
    if (GetNextToken() != T_END_OF_INPUT) {
      ReportError(JSONReader::JSON_UNEXPECTED_DATA_AFTER_ROOT, 1);
      return nullopt;
    }
  }

  return root;
}

JSONReader::JsonParseError JSONParser::error_code() const {
  return error_code_;
}

std::string JSONParser::GetErrorMessage() const {
  return FormatErrorMessage(error_line_, error_column_,
      JSONReader::ErrorCodeToString(error_code_));
}

int JSONParser::error_line() const {
  return error_line_;
}

int JSONParser::error_column() const {
  return error_column_;
}

// StringBuilder ///////////////////////////////////////////////////////////////

JSONParser::StringBuilder::StringBuilder() : StringBuilder(nullptr) {}

JSONParser::StringBuilder::StringBuilder(const char* pos)
    : pos_(pos), length_(0) {}

JSONParser::StringBuilder::~StringBuilder() = default;

JSONParser::StringBuilder& JSONParser::StringBuilder::operator=(
    StringBuilder&& other) = default;

void JSONParser::StringBuilder::Append(uint32_t point) {
  DCHECK(IsValidCharacter(point));

  if (point < kExtendedASCIIStart && !string_) {
    DCHECK_EQ(static_cast<char>(point), pos_[length_]);
    ++length_;
  } else {
    Convert();
    if (UNLIKELY(point == kUnicodeReplacementPoint)) {
      string_->append(kUnicodeReplacementString);
    } else {
      WriteUnicodeCharacter(point, &*string_);
    }
  }
}

void JSONParser::StringBuilder::Convert() {
  if (string_)
    return;
  string_.emplace(pos_, length_);
}

std::string JSONParser::StringBuilder::DestructiveAsString() {
  if (string_)
    return std::move(*string_);
  return std::string(pos_, length_);
}

// JSONParser private //////////////////////////////////////////////////////////

inline bool JSONParser::CanConsume(int length) {
  return static_cast<size_t>(index_) + length <= input_.length();
}

void JSONParser::NextChar() {
  CHECK(CanConsume(1));
  ++index_;
}

void JSONParser::NextNChars(int n) {
  CHECK(CanConsume(n));
  index_ += n;
}

const char* JSONParser::pos() {
  CHECK_LE(static_cast<size_t>(index_), input_.length());
  return input_.data() + index_;
}

char JSONParser::current_char() {
  // StringPiece internally bounds-checks.
  return input_[index_];
}

JSONParser::Token JSONParser::GetNextToken() {
  EatWhitespaceAndComments();
  if (!CanConsume(1))
    return T_END_OF_INPUT;

  switch (current_char()) {
    case '{':
      return T_OBJECT_BEGIN;
    case '}':
      return T_OBJECT_END;
    case '[':
      return T_ARRAY_BEGIN;
    case ']':
      return T_ARRAY_END;
    case '"':
      return T_STRING;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
    case '-':
      return T_NUMBER;
    case 't':
      return T_BOOL_TRUE;
    case 'f':
      return T_BOOL_FALSE;
    case 'n':
      return T_NULL;
    case ',':
      return T_LIST_SEPARATOR;
    case ':':
      return T_OBJECT_PAIR_SEPARATOR;
    default:
      return T_INVALID_TOKEN;
  }
}

void JSONParser::EatWhitespaceAndComments() {
  while (CanConsume(1)) {
    switch (current_char()) {
      case '\r':
      case '\n':
        index_last_line_ = index_;
        // Don't increment line_number_ twice for "\r\n".
        if (!(current_char() == '\n' &&
              index_ > 0 && input_[index_ - 1] == '\r')) {
          ++line_number_;
        }
        FALLTHROUGH;
      case ' ':
      case '\t':
        NextChar();
        break;
      case '/':
        if (!EatComment())
          return;
        break;
      default:
        return;
    }
  }
}

bool JSONParser::EatComment() {
  if (current_char() != '/' || !CanConsume(1))
    return false;

  NextChar();

  if (!CanConsume(1))
    return false;

  if (current_char() == '/') {
    // Single line comment, read to newline.
    while (CanConsume(1)) {
      if (current_char() == '\n' || current_char() == '\r')
        return true;
      NextChar();
    }
  } else if (current_char() == '*') {
    char previous_char = '\0';
    // Block comment, read until end marker.
    while (CanConsume(1)) {
      if (previous_char == '*' && current_char() == '/') {
        // EatWhitespaceAndComments will inspect pos(), which will still be on
        // the last / of the comment, so advance once more (which may also be
        // end of input).
        NextChar();
        return true;
      }
      previous_char = current_char();
      NextChar();
    }

    // If the comment is unterminated, GetNextToken will report T_END_OF_INPUT.
  }

  return false;
}

Optional<Value> JSONParser::ParseNextToken() {
  return ParseToken(GetNextToken());
}

Optional<Value> JSONParser::ParseToken(Token token) {
  switch (token) {
    case T_OBJECT_BEGIN:
      return ConsumeDictionary();
    case T_ARRAY_BEGIN:
      return ConsumeList();
    case T_STRING:
      return ConsumeString();
    case T_NUMBER:
      return ConsumeNumber();
    case T_BOOL_TRUE:
    case T_BOOL_FALSE:
    case T_NULL:
      return ConsumeLiteral();
    default:
      ReportError(JSONReader::JSON_UNEXPECTED_TOKEN, 1);
      return nullopt;
  }
}

Optional<Value> JSONParser::ConsumeDictionary() {
  // Attempt to alias 4KB of the buffer about to be read. Need to alias multiple
  // sites as the crashpad heuristics only grab a few hundred bytes in
  // front/behind heap pointers on the stack.
  // TODO(gab): Remove this after diagnosis of https://crbug.com/791487 is
  // complete.
  const char* initial_pos[16];
  for (size_t i = 0; i < arraysize(initial_pos); ++i)
    initial_pos[i] = pos() + i * 256;
  debug::Alias(&initial_pos);

  if (current_char() != '{') {
    ReportError(JSONReader::JSON_UNEXPECTED_TOKEN, 1);
    return nullopt;
  }

  StackMarker depth_check(max_depth_, &stack_depth_);
  if (depth_check.IsTooDeep()) {
    ReportError(JSONReader::JSON_TOO_MUCH_NESTING, 1);
    return nullopt;
  }

  std::vector<Value::DictStorage::value_type> dict_storage;

  NextChar();
  Token token = GetNextToken();
  while (token != T_OBJECT_END) {
    if (token != T_STRING) {
      ReportError(JSONReader::JSON_UNQUOTED_DICTIONARY_KEY, 1);
      return nullopt;
    }

    // First consume the key.
    StringBuilder key;
    if (!ConsumeStringRaw(&key)) {
      return nullopt;
    }

    // Read the separator.
    NextChar();
    token = GetNextToken();
    if (token != T_OBJECT_PAIR_SEPARATOR) {
      ReportError(JSONReader::JSON_SYNTAX_ERROR, 1);
      return nullopt;
    }

    // The next token is the value. Ownership transfers to |dict|.
    NextChar();
    Optional<Value> value = ParseNextToken();
    if (!value) {
      // ReportError from deeper level.
      return nullopt;
    }

    dict_storage.emplace_back(key.DestructiveAsString(),
                              std::make_unique<Value>(std::move(*value)));

    NextChar();
    token = GetNextToken();
    if (token == T_LIST_SEPARATOR) {
      NextChar();
      token = GetNextToken();
      if (token == T_OBJECT_END && !(options_ & JSON_ALLOW_TRAILING_COMMAS)) {
        ReportError(JSONReader::JSON_TRAILING_COMMA, 1);
        return nullopt;
      }
    } else if (token != T_OBJECT_END) {
      ReportError(JSONReader::JSON_SYNTAX_ERROR, 0);
      return nullopt;
    }
  }

  return Value(Value::DictStorage(std::move(dict_storage), KEEP_LAST_OF_DUPES));
}

Optional<Value> JSONParser::ConsumeList() {
  if (current_char() != '[') {
    ReportError(JSONReader::JSON_UNEXPECTED_TOKEN, 1);
    return nullopt;
  }

  StackMarker depth_check(max_depth_, &stack_depth_);
  if (depth_check.IsTooDeep()) {
    ReportError(JSONReader::JSON_TOO_MUCH_NESTING, 1);
    return nullopt;
  }

  Value::ListStorage list_storage;

  NextChar();
  Token token = GetNextToken();
  while (token != T_ARRAY_END) {
    Optional<Value> item = ParseToken(token);
    if (!item) {
      // ReportError from deeper level.
      return nullopt;
    }

    list_storage.push_back(std::move(*item));

    NextChar();
    token = GetNextToken();
    if (token == T_LIST_SEPARATOR) {
      NextChar();
      token = GetNextToken();
      if (token == T_ARRAY_END && !(options_ & JSON_ALLOW_TRAILING_COMMAS)) {
        ReportError(JSONReader::JSON_TRAILING_COMMA, 1);
        return nullopt;
      }
    } else if (token != T_ARRAY_END) {
      ReportError(JSONReader::JSON_SYNTAX_ERROR, 1);
      return nullopt;
    }
  }

  return Value(std::move(list_storage));
}

Optional<Value> JSONParser::ConsumeString() {
  StringBuilder string;
  if (!ConsumeStringRaw(&string))
    return nullopt;

  return Value(string.DestructiveAsString());
}

bool JSONParser::ConsumeStringRaw(StringBuilder* out) {
  if (current_char() != '"') {
    ReportError(JSONReader::JSON_UNEXPECTED_TOKEN, 1);
    return false;
  }

  // Strings are at minimum two characters: the surrounding double quotes.
  if (!CanConsume(2)) {
    ReportError(JSONReader::JSON_SYNTAX_ERROR, 1);
    return false;
  }
  NextChar();  // Read past opening quote.

  // StringBuilder will internally build a StringPiece unless a UTF-16
  // conversion occurs, at which point it will perform a copy into a
  // std::string.
  StringBuilder string(pos());

  // There must always be at least two characters left in the stream: the next
  // string character and the terminating closing quote.
  for (uint32_t next_char = 0; CanConsume(1); ++index_) {
    if (!ReadUnicodeCharacter(input_.data(),
                              static_cast<int32_t>(input_.length()),
                              &index_,
                              &next_char) ||
        !IsValidCharacter(next_char)) {
      if ((options_ & JSON_REPLACE_INVALID_CHARACTERS) == 0) {
        ReportError(JSONReader::JSON_UNSUPPORTED_ENCODING, 1);
        return false;
      }
      string.Append(kUnicodeReplacementPoint);
      continue;
    }

    if (next_char == '"') {
      *out = std::move(string);
      return true;
    }

    // If this character is not an escape sequence...
    if (next_char != '\\') {
      string.Append(next_char);
    } else {
      // And if it is an escape sequence, the input string will be adjusted
      // (either by combining the two characters of an encoded escape sequence,
      // or with a UTF conversion), so using StringPiece isn't possible -- force
      // a conversion.
      string.Convert();

      // Read past the escape '\' and ensure there's a character following.
      if (!CanConsume(1)) {
        ReportError(JSONReader::JSON_INVALID_ESCAPE, 0);
        return false;
      }
      NextChar();

      switch (current_char()) {
        // Allowed esape sequences:
        case 'x': {  // UTF-8 sequence.
          // UTF-8 \x escape sequences are not allowed in the spec, but they
          // are supported here for backwards-compatiblity with the old parser.
          if (!CanConsume(3)) {
            ReportError(JSONReader::JSON_INVALID_ESCAPE, 0);
            return false;
          }
          NextChar();  // Read past 'x'.

          int hex_digit = 0;
          if (!HexStringToInt(StringPiece(pos(), 2), &hex_digit) ||
              !IsValidCharacter(hex_digit)) {
            ReportError(JSONReader::JSON_INVALID_ESCAPE, 0);
            return false;
          }
          NextChar();

          string.Append(hex_digit);
          break;
        }
        case 'u': {  // UTF-16 sequence.
          // UTF units are of the form \uXXXX.
          // Read past the 'u'.
          NextChar();

          uint32_t code_point;
          if (!DecodeUTF16(&code_point)) {
            ReportError(JSONReader::JSON_INVALID_ESCAPE, 0);
            return false;
          }
          string.Append(code_point);

          break;
        }
        case '"':
          string.Append('"');
          break;
        case '\\':
          string.Append('\\');
          break;
        case '/':
          string.Append('/');
          break;
        case 'b':
          string.Append('\b');
          break;
        case 'f':
          string.Append('\f');
          break;
        case 'n':
          string.Append('\n');
          break;
        case 'r':
          string.Append('\r');
          break;
        case 't':
          string.Append('\t');
          break;
        case 'v':  // Not listed as valid escape sequence in the RFC.
          string.Append('\v');
          break;
        // All other escape squences are illegal.
        default:
          ReportError(JSONReader::JSON_INVALID_ESCAPE, 1);
          return false;
      }
    }
  }

  ReportError(JSONReader::JSON_SYNTAX_ERROR, 0);
  return false;
}

// Entry is at the first X in \uXXXX.
bool JSONParser::DecodeUTF16(uint32_t* out_code_point) {
  if (!CanConsume(4))
    return false;

  // Consume the UTF-16 code unit, which may be a high surrogate.
  int code_unit16_high = 0;
  if (!HexStringToInt(StringPiece(pos(), 4), &code_unit16_high))
    return false;

  // Only add 3, not 4, because at the end of this iteration, the parser has
  // finished working with the last digit of the UTF sequence, meaning that
  // the next iteration will advance to the next byte.
  NextNChars(3);

  // If this is a high surrogate, consume the next code unit to get the
  // low surrogate.
  if (CBU16_IS_SURROGATE(code_unit16_high)) {
    // Make sure this is the high surrogate. If not, it's an encoding
    // error.
    if (!CBU16_IS_SURROGATE_LEAD(code_unit16_high))
      return false;

    NextChar();  // Read past last HEX digit.

    // Make sure that the token has more characters to consume the
    // lower surrogate.
    if (!ConsumeIfMatch("\\u")) {
      return false;
    }
    NextChar();  // Read past 'u'.

    // Expect 4 HEX digits for the actual character.
    if (!CanConsume(4))
      return false;

    int code_unit16_low = 0;
    if (!HexStringToInt(StringPiece(pos(), 4), &code_unit16_low))
      return false;

    NextNChars(3);

    if (!CBU16_IS_TRAIL(code_unit16_low)) {
      return false;
    }

    uint32_t code_point =
        CBU16_GET_SUPPLEMENTARY(code_unit16_high, code_unit16_low);
    if (!IsValidCharacter(code_point))
      return false;

    *out_code_point = code_point;
  } else {
    // Not a surrogate.
    DCHECK(CBU16_IS_SINGLE(code_unit16_high));
    if (!IsValidCharacter(code_unit16_high)) {
      if ((options_ & JSON_REPLACE_INVALID_CHARACTERS) == 0) {
        return false;
      }
      *out_code_point = kUnicodeReplacementPoint;
      return true;
    }

    *out_code_point = code_unit16_high;
  }

  return true;
}

Optional<Value> JSONParser::ConsumeNumber() {
  const char* num_start = pos();
  const int start_index = index_;
  int end_index = start_index;

  if (current_char() == '-')
    NextChar();

  if (!ReadInt(false)) {
    ReportError(JSONReader::JSON_SYNTAX_ERROR, 1);
    return nullopt;
  }
  end_index = index_;

  // The optional fraction part.
  if (CanConsume(1) && current_char() == '.') {
    NextChar();
    if (!ReadInt(true)) {
      ReportError(JSONReader::JSON_SYNTAX_ERROR, 1);
      return nullopt;
    }
    end_index = index_;
  }

  // Optional exponent part.
  if (CanConsume(1) && (current_char() == 'e' || current_char() == 'E')) {
    NextChar();
    if (!CanConsume(1)) {
      ReportError(JSONReader::JSON_SYNTAX_ERROR, 1);
      return nullopt;
    }
    if (current_char() == '-' || current_char() == '+') {
      NextChar();
    }
    if (!ReadInt(true)) {
      ReportError(JSONReader::JSON_SYNTAX_ERROR, 1);
      return nullopt;
    }
    end_index = index_;
  }

  // ReadInt is greedy because numbers have no easily detectable sentinel,
  // so save off where the parser should be on exit (see Consume invariant at
  // the top of the header), then make sure the next token is one which is
  // valid.
  int exit_index = index_ - 1;

  switch (GetNextToken()) {
    case T_OBJECT_END:
    case T_ARRAY_END:
    case T_LIST_SEPARATOR:
    case T_END_OF_INPUT:
      break;
    default:
      ReportError(JSONReader::JSON_SYNTAX_ERROR, 1);
      return nullopt;
  }

  index_ = exit_index;

  StringPiece num_string(num_start, end_index - start_index);

  int num_int;
  if (StringToInt(num_string, &num_int))
    return Value(num_int);

  double num_double;
  if (StringToDouble(num_string.as_string(), &num_double) &&
      std::isfinite(num_double)) {
    return Value(num_double);
  }

  return nullopt;
}

bool JSONParser::ReadInt(bool allow_leading_zeros) {
  size_t len = 0;
  char first = 0;

  while (CanConsume(1)) {
    if (!IsAsciiDigit(current_char()))
      break;

    if (len == 0)
      first = current_char();

    ++len;
    NextChar();
  }

  if (len == 0)
    return false;

  if (!allow_leading_zeros && len > 1 && first == '0')
    return false;

  return true;
}

Optional<Value> JSONParser::ConsumeLiteral() {
  switch (current_char()) {
    case 't': {
      if (!ConsumeIfMatch("true")) {
        ReportError(JSONReader::JSON_SYNTAX_ERROR, 1);
        return nullopt;
      }
      return Value(true);
    }
    case 'f': {
      if (!ConsumeIfMatch("false")) {
        ReportError(JSONReader::JSON_SYNTAX_ERROR, 1);
        return nullopt;
      }
      return Value(false);
    }
    case 'n': {
      if (!ConsumeIfMatch("null")) {
        ReportError(JSONReader::JSON_SYNTAX_ERROR, 1);
        return nullopt;
      }
      return Value(Value::Type::NONE);
    }
    default:
      ReportError(JSONReader::JSON_UNEXPECTED_TOKEN, 1);
      return nullopt;
  }
}

bool JSONParser::ConsumeIfMatch(StringPiece match) {
  if (!CanConsume(match.size()) || match != StringPiece(pos(), match.size())) {
    return false;
  }
  // Per the Consume invariant, the parser state must be at the last byte of
  // the token on exit.
  NextNChars(match.size() - 1);
  return true;
}

void JSONParser::ReportError(JSONReader::JsonParseError code,
                             int column_adjust) {
  error_code_ = code;
  error_line_ = line_number_;
  error_column_ = index_ - index_last_line_ + column_adjust;
}

// static
std::string JSONParser::FormatErrorMessage(int line, int column,
                                           const std::string& description) {
  if (line || column) {
    return StringPrintf("Line: %i, column: %i, %s",
        line, column, description.c_str());
  }
  return description;
}

}  // namespace internal
}  // namespace base
