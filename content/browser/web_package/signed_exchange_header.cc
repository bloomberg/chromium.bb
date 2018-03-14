// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_header.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "components/cbor/cbor_reader.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "net/http/http_util.h"

namespace content {

namespace {

// IsStateful{Request,Response}Header return true if |name| is a stateful
// header field. Stateful header fields will cause validation failure of
// signed exchanges.
// https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#rfc.section.4.1
bool IsStatefulRequestHeader(base::StringPiece name) {
  const char* const kStatefulRequestHeaders[] = {
      "authorization", "cookie", "cookie2", "proxy-authorization",
      "sec-webSocket-key"};

  std::string lower_name(base::ToLowerASCII(name));
  for (const char* field : kStatefulRequestHeaders) {
    if (lower_name == field)
      return true;
  }
  return false;
}

bool IsStatefulResponseHeader(base::StringPiece name) {
  const char* const kStatefulResponseHeaders[] = {
      "authentication-control",
      "authentication-info",
      "optional-www-authenticate",
      "proxy-authenticate",
      "proxy-authentication-info",
      "sec-websocket-accept",
      "set-cookie",
      "set-cookie2",
      "setprofile",
      "www-authenticate",
  };

  std::string lower_name(base::ToLowerASCII(name));
  for (const char* field : kStatefulResponseHeaders) {
    if (lower_name == field)
      return true;
  }
  return false;
}

bool IsMethodCacheable(base::StringPiece method) {
  return method == "GET" || method == "HEAD" || method == "POST";
}

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
  base::StringPiece method_str = method_iter->second.GetBytestringAsString();
  // 3. If exchange’s request method is not safe (Section 4.2.1 of [RFC7231])
  // or not cacheable (Section 4.2.3 of [RFC7231]), return “invalid”.
  // [spec text]
  if (!net::HttpUtil::IsMethodSafe(method_str.as_string()) ||
      !IsMethodCacheable(method_str)) {
    DVLOG(1) << "Request method is not safe or not cacheable: " << method_str;
    return false;
  }
  out->set_request_method(method_str);

  for (const auto& it : request_map) {
    if (!it.first.is_bytestring() || !it.second.is_bytestring()) {
      DVLOG(1) << "Non-bytestring value in the request map";
      return false;
    }
    base::StringPiece name_str = it.first.GetBytestringAsString();
    if (name_str == kStatusKey)
      continue;
    // 4. If exchange’s headers contain a stateful header field, as defined in
    // Section 4.1, return “invalid”. [spec text]
    if (IsStatefulRequestHeader(name_str)) {
      DVLOG(1) << "Exchange contains stateful request header: " << name_str;
      return false;
    }
  }

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
    if (!net::HttpUtil::IsValidHeaderName(name_str)) {
      DVLOG(1) << "Invalid header name";
      return false;
    }
    // 4. If exchange’s headers contain a stateful header field, as defined in
    // Section 4.1, return “invalid”. [spec text]
    if (IsStatefulResponseHeader(name_str)) {
      DVLOG(1) << "Exchange contains stateful response header: " << name_str;
      return false;
    }

    base::StringPiece value_str = it.second.GetBytestringAsString();
    if (!net::HttpUtil::IsValidHeaderValue(value_str)) {
      DVLOG(1) << "Invalid header value";
      return false;
    }
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

scoped_refptr<net::HttpResponseHeaders>
SignedExchangeHeader::BuildHttpResponseHeaders() const {
  std::string header_str("HTTP/1.1 ");
  header_str.append(base::NumberToString(response_code()));
  header_str.append(" ");
  header_str.append(net::GetHttpReasonPhrase(response_code()));
  header_str.append(" \r\n");
  for (const auto& it : response_headers()) {
    header_str.append(it.first);
    header_str.append(": ");
    header_str.append(it.second);
    header_str.append("\r\n");
  }
  header_str.append("\r\n");
  return base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(header_str.c_str(), header_str.size()));
}

}  // namespace content
