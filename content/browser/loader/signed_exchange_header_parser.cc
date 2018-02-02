// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/signed_exchange_header_parser.h"

#include <map>
#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"

namespace content {

namespace {

// This covers the characters allowed in Numbers, Labels, and Binary Content.
constexpr char kTokenChars[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_+-*/";

struct ParameterisedLabel {
  std::string label;
  std::map<std::string, std::string> params;
};

// Parser for (a subset of) Structured Headers defined in [SH].
// [SH] https://tools.ietf.org/html/draft-ietf-httpbis-header-structure-02
class StructuredHeaderParser {
 public:
  explicit StructuredHeaderParser(const base::StringPiece& str)
      : input_(str), failed_(false) {}

  bool ParsedSuccessfully() const { return !failed_ && input_.empty(); }

  // Parses a List ([SH] 4.8) of Strings.
  void ParseStringList(std::vector<std::string>* values) {
    values->push_back(ReadString());
    while (!failed_) {
      SkipWhitespaces();
      if (!ConsumeChar(','))
        break;
      SkipWhitespaces();
      values->push_back(ReadString());
    }
  }

  // Parses a List ([SH] 4.8) of Parameterised Labels.
  void ParseParameterisedLabelList(std::vector<ParameterisedLabel>* values) {
    values->push_back(ParameterisedLabel());
    ParseParameterisedLabel(&values->back());
    while (!failed_) {
      SkipWhitespaces();
      if (!ConsumeChar(','))
        break;
      SkipWhitespaces();
      values->push_back(ParameterisedLabel());
      ParseParameterisedLabel(&values->back());
    }
  }

  // Parses a Parameterised Label ([SH] 4.4).
  void ParseParameterisedLabel(ParameterisedLabel* out) {
    std::string label = ReadToken();
    if (label.empty()) {
      DVLOG(1) << "ParseParameterisedLabel: Label expected, got '"
               << input_.front() << "'";
      failed_ = true;
      return;
    }
    out->label = label;

    while (!failed_) {
      SkipWhitespaces();
      if (!ConsumeChar(';'))
        break;
      SkipWhitespaces();

      std::string name = ReadToken();
      if (name.empty()) {
        DVLOG(1) << "ParseParameterisedLabel: Label expected, got '"
                 << input_.front() << "'";
        failed_ = true;
        return;
      }
      std::string value;
      if (ConsumeChar('='))
        value = ReadItem();
      if (!out->params.insert(std::make_pair(name, value)).second) {
        DVLOG(1) << "ParseParameterisedLabel: duplicated parameter: " << name;
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

  // [SH] 4.2. Strings
  std::string ReadString() {
    std::string s;
    if (!ConsumeChar('"')) {
      DVLOG(1) << "ReadString: '\"' expected, got '" << input_.front() << "'";
      failed_ = true;
      return s;
    }
    while (input_.front() != '"') {
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
    input_.remove_prefix(1);  // Consume '"'
    return s;
  }

  // [SH] 4.5. Binary Content
  std::string ReadBinary() {
    if (!ConsumeChar('*')) {
      DVLOG(1) << "ReadBinary: '*' expected, got '" << input_.front() << "'";
      failed_ = true;
      return std::string();
    }
    std::string base64 = ReadToken();
    // Binary Content does not have padding, so we have to add it.
    base64.resize((base64.size() + 3) / 4 * 4, '=');
    std::string binary;
    if (!base::Base64Decode(base64, &binary)) {
      DVLOG(1) << "ReadBinary: failed to decode base64: " << base64;
      failed_ = true;
    }
    return binary;
  }

  // [SH] 4.6. Items
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
      default:  // label or number
        return ReadToken();
    }
  }

  base::StringPiece input_;
  bool failed_;
};

}  // namespace

base::Optional<std::vector<std::string>>
SignedExchangeHeaderParser::ParseSignedHeaders(
    const std::string& signed_headers_str) {
  std::vector<std::string> headers;
  StructuredHeaderParser parser(signed_headers_str);
  parser.ParseStringList(&headers);
  if (!parser.ParsedSuccessfully())
    return base::nullopt;
  return headers;
}

base::Optional<std::vector<SignedExchangeHeaderParser::Signature>>
SignedExchangeHeaderParser::ParseSignature(const std::string& signature_str) {
  StructuredHeaderParser parser(signature_str);
  std::vector<ParameterisedLabel> values;
  parser.ParseParameterisedLabelList(&values);
  if (!parser.ParsedSuccessfully())
    return base::nullopt;

  std::vector<Signature> signatures;
  signatures.reserve(values.size());
  for (auto& value : values) {
    signatures.push_back(Signature());
    Signature& sig = signatures.back();
    sig.label = value.label;
    sig.sig = value.params["sig"];
    sig.integrity = value.params["integrity"];
    sig.certUrl = value.params["certUrl"];
    sig.certSha256 = value.params["certSha256"];
    sig.ed25519Key = value.params["ed25519Key"];
    sig.validityUrl = value.params["validityUrl"];

    if (!base::StringToUint64(value.params["date"], &sig.date)) {
      DVLOG(1) << "ParseSignature: 'date' parameter is not a number: "
               << sig.date;
      return base::nullopt;
    }
    if (!base::StringToUint64(value.params["expires"], &sig.expires)) {
      DVLOG(1) << "ParseSignature: 'expires' parameter is not a number:"
               << sig.expires;
      return base::nullopt;
    }

    bool hasCert = !sig.certUrl.empty() && !sig.certSha256.empty();
    bool hasEd25519Key = !sig.ed25519Key.empty();
    if (sig.sig.empty() || sig.integrity.empty() || sig.validityUrl.empty() ||
        (!hasCert && !hasEd25519Key)) {
      DVLOG(1) << "ParseSignature: incomplete signature";
      return base::nullopt;
    }
    if (hasCert && hasEd25519Key) {
      DVLOG(1) << "ParseSignature: signature has both certUrl and ed25519Key";
      return base::nullopt;
    }
  }
  return signatures;
}

SignedExchangeHeaderParser::Signature::Signature() = default;
SignedExchangeHeaderParser::Signature::Signature(const Signature& other) =
    default;
SignedExchangeHeaderParser::Signature::~Signature() = default;

}  // namespace content
