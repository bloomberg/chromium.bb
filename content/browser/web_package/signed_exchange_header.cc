// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_header.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "components/cbor/cbor_reader.h"
#include "content/browser/web_package/signed_exchange_consts.h"

namespace content {

namespace {

bool ParseRequestMap(const cbor::CBORValue& value, SignedExchangeHeader* out) {
  if (!value.is_map()) {
    DVLOG(1) << "Expected request map, got non-map type "
             << static_cast<int>(value.type());
    return false;
  }

  const cbor::CBORValue::MapValue& request_map = value.GetMap();

  auto url_iter = request_map.find(
      cbor::CBORValue(kUrlKey, cbor::CBORValue::Type::BYTE_STRING));
  if (url_iter == request_map.end() || !url_iter->second.is_bytestring()) {
    DVLOG(1) << kUrlKey << " is not found or not a bytestring";
    return false;
  }
  out->set_request_url(GURL(url_iter->second.GetBytestringAsString()));

  auto method_iter = request_map.find(
      cbor::CBORValue(kMethodKey, cbor::CBORValue::Type::BYTE_STRING));
  if (method_iter == request_map.end() ||
      !method_iter->second.is_bytestring()) {
    DVLOG(1) << kMethodKey << " is not found or not a bytestring";
    return false;
  }
  out->set_request_method(method_iter->second.GetBytestringAsString());

  return true;
}

bool ParseResponseMap(const cbor::CBORValue& value, SignedExchangeHeader* out) {
  if (!value.is_map()) {
    DVLOG(1) << "Expected response headers map, got non-map type "
             << static_cast<int>(value.type());
    return false;
  }

  const cbor::CBORValue::MapValue& response_map = value.GetMap();
  auto status_iter = response_map.find(
      cbor::CBORValue(kStatusKey, cbor::CBORValue::Type::BYTE_STRING));
  if (status_iter == response_map.end() ||
      !status_iter->second.is_bytestring()) {
    DVLOG(1) << kStatusKey << " is not found or not a bytestring.";
    return false;
  }
  base::StringPiece response_code_str =
      status_iter->second.GetBytestringAsString();
  int response_code;
  if (!base::StringToInt(response_code_str, &response_code)) {
    DVLOG(1) << "Failed to parse status code \"" << response_code_str
             << "\" to string.";
    return false;
  }
  out->set_response_code(static_cast<net::HttpStatusCode>(response_code));

  for (const auto& it : response_map) {
    if (!it.first.is_bytestring() || !it.second.is_bytestring()) {
      DVLOG(1) << "Non-bytestring value in the response map";
      return false;
    }
    base::StringPiece name_str = it.first.GetBytestringAsString();
    if (name_str == kStatusKey)
      continue;

    base::StringPiece value_str = it.second.GetBytestringAsString();
    out->AddResponseHeader(name_str, value_str);
  }

  return true;
}

}  // namespace

constexpr size_t SignedExchangeHeader::kEncodedHeaderLengthInBytes;

// static
size_t SignedExchangeHeader::ParseHeadersLength(
    base::span<const uint8_t> input) {
  DCHECK_EQ(input.size(), SignedExchangeHeader::kEncodedHeaderLengthInBytes);
  return static_cast<size_t>(input[0]) << 16 |
         static_cast<size_t>(input[1]) << 8 | static_cast<size_t>(input[2]);
}

// static
base::Optional<SignedExchangeHeader> SignedExchangeHeader::Parse(
    base::span<const uint8_t> input) {
  cbor::CBORReader::DecoderError error;
  base::Optional<cbor::CBORValue> value = cbor::CBORReader::Read(input, &error);
  if (!value.has_value()) {
    DVLOG(1) << "Failed to decode CBORValue: "
             << cbor::CBORReader::ErrorCodeToString(error);
    return base::nullopt;
  }
  if (!value->is_array()) {
    DVLOG(1) << "Expected top-level CBORValue to be an array, but got: "
             << static_cast<int>(value->type());
    return base::nullopt;
  }

  const cbor::CBORValue::ArrayValue& top_level_array = value->GetArray();
  constexpr size_t kTopLevelArraySize = 2;
  if (top_level_array.size() != kTopLevelArraySize) {
    DVLOG(1) << "Expected top-level array to have 2 elements, but got "
             << top_level_array.size() << " elements.";
    return base::nullopt;
  }

  SignedExchangeHeader ret;

  if (!ParseRequestMap(top_level_array[0], &ret) ||
      !ParseResponseMap(top_level_array[1], &ret))
    return base::nullopt;

  auto signature_iter = ret.response_headers_.find(kSignature);
  if (signature_iter == ret.response_headers_.end())
    return base::nullopt;

  base::Optional<std::vector<SignedExchangeHeaderParser::Signature>>
      signatures =
          SignedExchangeHeaderParser::ParseSignature(signature_iter->second);
  if (!signatures || signatures->empty())
    return base::nullopt;

  ret.signature_ = (*signatures)[0];

  return std::move(ret);
}

SignedExchangeHeader::SignedExchangeHeader() = default;
SignedExchangeHeader::SignedExchangeHeader(const SignedExchangeHeader&) =
    default;
SignedExchangeHeader::SignedExchangeHeader(SignedExchangeHeader&&) = default;
SignedExchangeHeader::~SignedExchangeHeader() = default;
SignedExchangeHeader& SignedExchangeHeader::operator=(SignedExchangeHeader&&) =
    default;

void SignedExchangeHeader::AddResponseHeader(base::StringPiece name,
                                             base::StringPiece value) {
  std::string name_string;
  std::string value_string;
  name.CopyToString(&name_string);
  value.CopyToString(&value_string);
  response_headers_[name_string] = value_string;
}

}  // namespace content
