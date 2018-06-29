// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_envelope.h"

#include <utility>

#include "base/callback.h"
#include "base/format_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "components/cbor/cbor_reader.h"
#include "content/browser/web_package/signed_exchange_consts.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "net/http/http_util.h"
#include "url/origin.h"

namespace content {

namespace {

// IsStateful{Request,Response}Header return true if |name| is a stateful
// header field. Stateful header fields will cause validation failure of
// signed exchanges.
// Note that |name| must be lower-cased.
// https://wicg.github.io/webpackage/draft-yasskin-httpbis-origin-signed-exchanges-impl.html#stateful-headers
bool IsStatefulRequestHeader(base::StringPiece name) {
  DCHECK_EQ(name, base::ToLowerASCII(name));

  const char* const kStatefulRequestHeaders[] = {
      "authorization", "cookie", "cookie2", "proxy-authorization",
      "sec-webSocket-key"};

  for (const char* field : kStatefulRequestHeaders) {
    if (name == field)
      return true;
  }
  return false;
}

bool IsStatefulResponseHeader(base::StringPiece name) {
  DCHECK_EQ(name, base::ToLowerASCII(name));

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

  for (const char* field : kStatefulResponseHeaders) {
    if (name == field)
      return true;
  }
  return false;
}

bool IsMethodCacheable(base::StringPiece method) {
  return method == "GET" || method == "HEAD" || method == "POST";
}

bool ParseRequestMap(const cbor::CBORValue& value,
                     SignedExchangeEnvelope* out,
                     SignedExchangeDevToolsProxy* devtools_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"), "ParseRequestMap");
  if (!value.is_map()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        base::StringPrintf(
            "Expected request map, got non-map type. Actual type: %d",
            static_cast<int>(value.type())));
    return false;
  }

  const cbor::CBORValue::MapValue& request_map = value.GetMap();

  auto url_iter = request_map.find(
      cbor::CBORValue(kUrlKey, cbor::CBORValue::Type::BYTE_STRING));
  if (url_iter == request_map.end() || !url_iter->second.is_bytestring()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, ":url is not found or not a bytestring.");
    return false;
  }
  out->set_request_url(GURL(url_iter->second.GetBytestringAsString()));
  if (!out->request_url().is_valid()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(devtools_proxy,
                                                    ":url is not a valid URL.");
    return false;
  }
  if (out->request_url().has_ref()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, ":url can't have a fragment.");
    return false;
  }
  if (!out->request_url().SchemeIs("https")) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, ":url scheme must be 'https'.");
    return false;
  }

  auto method_iter = request_map.find(
      cbor::CBORValue(kMethodKey, cbor::CBORValue::Type::BYTE_STRING));
  if (method_iter == request_map.end() ||
      !method_iter->second.is_bytestring()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, ":method is not found or not a bytestring.");
    return false;
  }
  base::StringPiece method_str = method_iter->second.GetBytestringAsString();
  // 3. If exchange’s request method is not safe (Section 4.2.1 of [RFC7231])
  // or not cacheable (Section 4.2.3 of [RFC7231]), return “invalid”.
  // [spec text]
  if (!net::HttpUtil::IsMethodSafe(method_str.as_string()) ||
      !IsMethodCacheable(method_str)) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        base::StringPrintf(
            "Request method is not safe or not cacheable. method: %s",
            method_str.as_string().c_str()));
    return false;
  }
  out->set_request_method(method_str);

  for (const auto& it : request_map) {
    if (!it.first.is_bytestring() || !it.second.is_bytestring()) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy, "Non-bytestring value in the request map.");
      return false;
    }
    base::StringPiece name_str = it.first.GetBytestringAsString();
    if (name_str == kUrlKey || name_str == kMethodKey)
      continue;

    // TODO(kouhei): Add spec ref here once
    // https://github.com/WICG/webpackage/issues/161 is resolved.
    if (name_str != base::ToLowerASCII(name_str)) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy,
          base::StringPrintf(
              "Request header name should be lower-cased. header name: %s",
              name_str.as_string().c_str()));
      return false;
    }

    // 4. If exchange’s headers contain a stateful header field, as defined in
    // Section 4.1, return “invalid”. [spec text]
    if (IsStatefulRequestHeader(name_str)) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy,
          base::StringPrintf(
              "Exchange contains stateful request header. header name: %s",
              name_str.as_string().c_str()));
      return false;
    }
  }
  return true;
}

bool ParseResponseMap(const cbor::CBORValue& value,
                      SignedExchangeEnvelope* out,
                      SignedExchangeDevToolsProxy* devtools_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"), "ParseResponseMap");
  if (!value.is_map()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        base::StringPrintf(
            "Expected request map, got non-map type. Actual type: %d",
            static_cast<int>(value.type())));
    return false;
  }

  const cbor::CBORValue::MapValue& response_map = value.GetMap();
  auto status_iter = response_map.find(
      cbor::CBORValue(kStatusKey, cbor::CBORValue::Type::BYTE_STRING));
  if (status_iter == response_map.end() ||
      !status_iter->second.is_bytestring()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, ":status is not found or not a bytestring.");
    return false;
  }
  base::StringPiece response_code_str =
      status_iter->second.GetBytestringAsString();
  int response_code;
  if (!base::StringToInt(response_code_str, &response_code)) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, "Failed to parse status code to integer.");
    return false;
  }
  out->set_response_code(static_cast<net::HttpStatusCode>(response_code));

  for (const auto& it : response_map) {
    if (!it.first.is_bytestring() || !it.second.is_bytestring()) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy, "Non-bytestring value in the response map.");
      return false;
    }
    base::StringPiece name_str = it.first.GetBytestringAsString();
    if (name_str == kStatusKey)
      continue;
    if (!net::HttpUtil::IsValidHeaderName(name_str)) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy,
          base::StringPrintf("Invalid header name. header_name: %s",
                             name_str.as_string().c_str()));
      return false;
    }

    // TODO(kouhei): Add spec ref here once
    // https://github.com/WICG/webpackage/issues/161 is resolved.
    if (name_str != base::ToLowerASCII(name_str)) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy,
          base::StringPrintf(
              "Response header name should be lower-cased. header_name: %s",
              name_str.as_string().c_str()));
      return false;
    }

    // 4. If exchange’s headers contain a stateful header field, as defined in
    // Section 4.1, return “invalid”. [spec text]
    if (IsStatefulResponseHeader(name_str)) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy,
          base::StringPrintf(
              "Exchange contains stateful response header. header_name: %s",
              name_str.as_string().c_str()));
      return false;
    }

    base::StringPiece value_str = it.second.GetBytestringAsString();
    if (!net::HttpUtil::IsValidHeaderValue(value_str)) {
      signed_exchange_utils::ReportErrorAndTraceEvent(devtools_proxy,
                                                      "Invalid header value.");
      return false;
    }
    if (!out->AddResponseHeader(name_str, value_str)) {
      signed_exchange_utils::ReportErrorAndTraceEvent(
          devtools_proxy,
          base::StringPrintf("Duplicate header value. header_name: %s",
                             name_str.as_string().c_str()));
      return false;
    }
  }
  return true;
}

}  // namespace

// static
base::Optional<SignedExchangeEnvelope> SignedExchangeEnvelope::Parse(
    base::StringPiece signature_header_field,
    base::span<const uint8_t> cbor_header,
    SignedExchangeDevToolsProxy* devtools_proxy) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "SignedExchangeEnvelope::Parse");
  cbor::CBORReader::DecoderError error;
  base::Optional<cbor::CBORValue> value =
      cbor::CBORReader::Read(cbor_header, &error);
  if (!value.has_value()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        base::StringPrintf("Failed to decode CBORValue. CBOR error: %s",
                           cbor::CBORReader::ErrorCodeToString(error)));
    return base::nullopt;
  }
  if (!value->is_array()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        base::StringPrintf(
            "Expected top-level CBORValue to be an array. Actual type : %d",
            static_cast<int>(value->type())));
    return base::nullopt;
  }

  const cbor::CBORValue::ArrayValue& top_level_array = value->GetArray();
  constexpr size_t kTopLevelArraySize = 2;
  if (top_level_array.size() != kTopLevelArraySize) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy,
        base::StringPrintf("Expected top-level array to have 2 elements. "
                           "Actual element count: %" PRIuS,
                           top_level_array.size()));
    return base::nullopt;
  }

  SignedExchangeEnvelope ret;

  if (!ParseRequestMap(top_level_array[0], &ret, devtools_proxy)) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, "Failed to parse request map.");
    return base::nullopt;
  }
  if (!ParseResponseMap(top_level_array[1], &ret, devtools_proxy)) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, "Failed to parse response map.");
    return base::nullopt;
  }

  base::Optional<std::vector<SignedExchangeSignatureHeaderField::Signature>>
      signatures = SignedExchangeSignatureHeaderField::ParseSignature(
          signature_header_field, devtools_proxy);
  if (!signatures || signatures->empty()) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, "Failed to parse signature header field.");
    return base::nullopt;
  }

  // TODO(https://crbug.com/850475): Support multiple signatures.
  ret.signature_ = (*signatures)[0];

  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#cross-origin-trust
  // If the signature’s “validity-url” parameter is not same-origin with
  // exchange’s effective request URI (Section 5.5 of [RFC7230]), return
  // “invalid” [spec text]
  const GURL request_url = ret.request_url();
  const GURL validity_url = ret.signature().validity_url;
  if (!url::IsSameOriginWith(request_url, validity_url)) {
    signed_exchange_utils::ReportErrorAndTraceEvent(
        devtools_proxy, "Validity URL must be same-origin with request URL.");
    return base::nullopt;
  }

  return std::move(ret);
}

SignedExchangeEnvelope::SignedExchangeEnvelope() = default;
SignedExchangeEnvelope::SignedExchangeEnvelope(const SignedExchangeEnvelope&) =
    default;
SignedExchangeEnvelope::SignedExchangeEnvelope(SignedExchangeEnvelope&&) =
    default;
SignedExchangeEnvelope::~SignedExchangeEnvelope() = default;
SignedExchangeEnvelope& SignedExchangeEnvelope::operator=(
    SignedExchangeEnvelope&&) = default;

bool SignedExchangeEnvelope::AddResponseHeader(base::StringPiece name,
                                               base::StringPiece value) {
  std::string name_str = name.as_string();
  DCHECK_EQ(name_str, base::ToLowerASCII(name))
      << "Response header names should be always lower-cased.";
  if (response_headers_.find(name_str) != response_headers_.end())
    return false;

  response_headers_.emplace(std::move(name_str), value.as_string());
  return true;
}

scoped_refptr<net::HttpResponseHeaders>
SignedExchangeEnvelope::BuildHttpResponseHeaders() const {
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
