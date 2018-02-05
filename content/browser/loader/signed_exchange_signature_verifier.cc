// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/signed_exchange_signature_verifier.h"

#include "base/strings/string_number_conversions.h"
#include "components/cbor/cbor_values.h"
#include "components/cbor/cbor_writer.h"
#include "content/browser/loader/signed_exchange_header_parser.h"

namespace content {

namespace {

constexpr char kSignedHeadersName[] = "signed-headers";

// TODO(crbug.com/803774): Share these string constants with the parser
constexpr char kUrlKey[] = ":url";
constexpr char kMethodKey[] = ":method";
constexpr char kStatusKey[] = ":status";

base::Optional<cbor::CBORValue> GenerateCanonicalRequestCBOR(
    const SignedExchangeSignatureVerifier::Input& input) {
  cbor::CBORValue::MapValue map;
  map.insert_or_assign(
      cbor::CBORValue(kMethodKey, cbor::CBORValue::Type::BYTE_STRING),
      cbor::CBORValue(input.method, cbor::CBORValue::Type::BYTE_STRING));
  map.insert_or_assign(
      cbor::CBORValue(kUrlKey, cbor::CBORValue::Type::BYTE_STRING),
      cbor::CBORValue(input.url, cbor::CBORValue::Type::BYTE_STRING));

  return cbor::CBORValue(map);
}

base::Optional<cbor::CBORValue> GenerateCanonicalResponseCBOR(
    const SignedExchangeSignatureVerifier::Input& input) {
  const auto& headers = input.response_headers;

  auto it = headers.find(kSignedHeadersName);
  if (it == headers.end()) {
    DVLOG(1) << "The Signed-Headers http header not found";
    return base::nullopt;
  }
  const std::string& signed_header_value = it->second;

  base::Optional<std::vector<std::string>> signed_headers =
      SignedExchangeHeaderParser::ParseSignedHeaders(signed_header_value);
  if (!signed_headers)
    return base::nullopt;

  cbor::CBORValue::MapValue map;
  std::string response_code_str = base::NumberToString(input.response_code);
  map.insert_or_assign(
      cbor::CBORValue(kStatusKey, cbor::CBORValue::Type::BYTE_STRING),
      cbor::CBORValue(response_code_str, cbor::CBORValue::Type::BYTE_STRING));

  for (const std::string& name : *signed_headers) {
    auto it = headers.find(name);
    if (it == headers.end()) {
      DVLOG(1) << "Signed header \"" << name
               << "\" expected, but not found in response_headers.";
      return base::nullopt;
    }
    const std::string& value = it->second;
    map.insert_or_assign(
        cbor::CBORValue(name, cbor::CBORValue::Type::BYTE_STRING),
        cbor::CBORValue(value, cbor::CBORValue::Type::BYTE_STRING));
  }

  return cbor::CBORValue(map);
}

// Generate CBORValue from |input| as specified in:
// https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#rfc.section.3.4
base::Optional<cbor::CBORValue> GenerateCanonicalExchangeHeadersCBOR(
    const SignedExchangeSignatureVerifier::Input& input) {
  auto req_val = GenerateCanonicalRequestCBOR(input);
  if (!req_val)
    return base::nullopt;
  auto res_val = GenerateCanonicalResponseCBOR(input);
  if (!res_val)
    return base::nullopt;

  cbor::CBORValue::ArrayValue array;
  array.push_back(std::move(*req_val));
  array.push_back(std::move(*res_val));
  return cbor::CBORValue(array);
}

}  // namespace

SignedExchangeSignatureVerifier::Input::Input() = default;

SignedExchangeSignatureVerifier::Input::~Input() = default;

bool SignedExchangeSignatureVerifier::Verify(const Input& input) {
  NOTIMPLEMENTED();
  return true;
}

base::Optional<std::vector<uint8_t>>
SignedExchangeSignatureVerifier::EncodeCanonicalExchangeHeaders(
    const SignedExchangeSignatureVerifier::Input& input) {
  base::Optional<cbor::CBORValue> cbor_val =
      GenerateCanonicalExchangeHeadersCBOR(input);
  if (!cbor_val)
    return base::nullopt;

  return cbor::CBORWriter::Write(*cbor_val);
}

}  // namespace content
