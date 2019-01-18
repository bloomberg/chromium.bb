// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/http_structured_header.h"

#include "base/base64.h"
#include "base/strings/string_util.h"

namespace content {
namespace http_structured_header {

namespace {

// This covers the characters allowed in Integers and Identifiers.
// https://tools.ietf.org/html/draft-ietf-httpbis-header-structure-04#section-4.5
// https://tools.ietf.org/html/draft-ietf-httpbis-header-structure-04#section-4.8
constexpr char kTokenChars[] = "0123456789abcdefghijklmnopqrstuvwxyz_-*/";

// Parser for (a subset of) Structured Headers defined in [SH].
// [SH] https://tools.ietf.org/html/draft-ietf-httpbis-header-structure-04
class StructuredHeaderParser {
 public:
  explicit StructuredHeaderParser(const base::StringPiece& str)
      : input_(str), failed_(false) {}

  // Callers should call this after ParseSomething(), to check if parser has
  // consumed all the input successfully.
  bool ParsedSuccessfully() const { return !failed_ && input_.empty(); }

  // Parses a Parameterised List ([SH] 4.3).
  void ParseParameterisedList(std::vector<ParameterisedIdentifier>* values) {
    values->push_back(ParameterisedIdentifier());
    ParseParameterisedIdentifier(&values->back());
    while (!failed_) {
      SkipWhitespaces();
      if (!ConsumeChar(','))
        break;
      SkipWhitespaces();
      values->push_back(ParameterisedIdentifier());
      ParseParameterisedIdentifier(&values->back());
    }
  }

  // Parses a Parameterised Identifier ([SH] 4.3.2).
  void ParseParameterisedIdentifier(ParameterisedIdentifier* out) {
    std::string identifier = ReadToken();
    if (identifier.empty()) {
      DVLOG(1) << "ParseParameterisedIdentifier: Identifier expected, got '"
               << input_.front() << "'";
      failed_ = true;
      return;
    }
    out->identifier = identifier;

    while (!failed_) {
      SkipWhitespaces();
      if (!ConsumeChar(';'))
        break;
      SkipWhitespaces();

      std::string name = ReadToken();
      if (name.empty()) {
        DVLOG(1) << "ParseParameterisedIdentifier: Identifier expected, got '"
                 << input_.front() << "'";
        failed_ = true;
        return;
      }
      std::string value;
      if (ConsumeChar('='))
        value = ReadItem();
      if (!out->params.insert(std::make_pair(name, value)).second) {
        DVLOG(1) << "ParseParameterisedIdentifier: duplicated parameter: "
                 << name;
        failed_ = true;
        return;
      }
    }
  }

 private:
  void SkipWhitespaces() {
    input_ = base::TrimWhitespaceASCII(input_, base::TRIM_LEADING);
  }

  std::string ReadToken() {
    size_t len = input_.find_first_not_of(kTokenChars);
    if (len == base::StringPiece::npos)
      len = input_.size();
    std::string token(input_.substr(0, len));
    input_.remove_prefix(len);
    return token;
  }

  bool ConsumeChar(char expected) {
    if (!input_.empty() && input_.front() == expected) {
      input_.remove_prefix(1);
      return true;
    }
    return false;
  }

  // [SH] 4.7. Strings
  std::string ReadString() {
    std::string s;
    if (!ConsumeChar('"')) {
      DVLOG(1) << "ReadString: '\"' expected, got '" << input_.front() << "'";
      failed_ = true;
      return s;
    }
    while (!ConsumeChar('"')) {
      size_t len = input_.find_first_of("\"\\");
      if (len == base::StringPiece::npos) {
        DVLOG(1) << "ReadString: missing closing '\"'";
        failed_ = true;
        return s;
      }
      s.append(std::string(input_.substr(0, len)));
      input_.remove_prefix(len);
      if (ConsumeChar('\\')) {
        if (input_.empty()) {
          DVLOG(1) << "ReadString: backslash at string end";
          failed_ = true;
          return s;
        }
        s.push_back(input_.front());
        input_.remove_prefix(1);
      }
    }
    return s;
  }

  // [SH] 4.9. Binary Content
  std::string ReadBinary() {
    if (!ConsumeChar('*')) {
      DVLOG(1) << "ReadBinary: '*' expected, got '" << input_.front() << "'";
      failed_ = true;
      return std::string();
    }
    size_t len = input_.find('*');
    if (len == base::StringPiece::npos) {
      DVLOG(1) << "ReadBinary: missing closing '*'";
      failed_ = true;
      return std::string();
    }
    base::StringPiece base64 = input_.substr(0, len);
    std::string binary;
    if (!base::Base64Decode(base64, &binary)) {
      DVLOG(1) << "ReadBinary: failed to decode base64: " << base64;
      failed_ = true;
    }
    input_.remove_prefix(len);
    ConsumeChar('*');
    return binary;
  }

  // [SH] 4.4. Items
  std::string ReadItem() {
    if (input_.empty()) {
      DVLOG(1) << "ReadItem: unexpected EOF";
      failed_ = true;
      return std::string();
    }
    switch (input_.front()) {
      case '"':
        return ReadString();
      case '*':
        return ReadBinary();
      default:  // identifier or integer
        return ReadToken();
    }
  }

  base::StringPiece input_;
  bool failed_;
  DISALLOW_COPY_AND_ASSIGN(StructuredHeaderParser);
};

}  // namespace

ParameterisedIdentifier::ParameterisedIdentifier() = default;
ParameterisedIdentifier::ParameterisedIdentifier(
    const ParameterisedIdentifier&) = default;
ParameterisedIdentifier::~ParameterisedIdentifier() = default;

base::Optional<ParameterisedList> ParseParameterisedList(
    const base::StringPiece& str) {
  StructuredHeaderParser parser(str);
  ParameterisedList values;
  parser.ParseParameterisedList(&values);
  if (parser.ParsedSuccessfully())
    return base::make_optional(std::move(values));
  return base::nullopt;
}

}  // namespace http_structured_header
}  // namespace content
