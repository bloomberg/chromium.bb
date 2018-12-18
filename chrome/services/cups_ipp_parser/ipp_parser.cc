// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_ipp_parser/ipp_parser.h"

#include <cups/cups.h>
#include <memory>
#include <utility>

#include "base/optional.h"
#include "base/strings/string_split.h"
#include "chrome/services/cups_ipp_parser/ipp_parser_util.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"

namespace chrome {
namespace {

const char kCarriage[] = "\r\n";
const char kStatusDelimiter[] = " ";

using HttpHeader = std::pair<std::string, std::string>;

// Log debugging error and send empty response, signalling error.
void Fail(const std::string& error_log, IppParser::ParseIppCallback cb) {
  DVLOG(1) << "IPP Parser Error: " << error_log;
  std::move(cb).Run(mojom::IppParserResult::FAILURE, nullptr);
  return;
}

// Returns a parsed request line on success, empty Optional on failure.
base::Optional<std::vector<std::string>> ParseRequestLine(
    base::StringPiece request) {
  auto end_of_request_line = request.find(kCarriage);
  if (end_of_request_line == std::string::npos) {
    return base::nullopt;
  }

  // Pare down request to just the HTTP request_line
  request.remove_suffix(request.size() - end_of_request_line);

  std::vector<std::string> terms = base::SplitString(
      request, kStatusDelimiter, base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (terms.size() != 3) {
    return base::nullopt;
  }

  // All CUPS IPP request lines must be in format 'POST *endpoint* HTTP/1.1'.
  // TODO(crbug/831928): parse printing *endpoint*
  if (terms[0] != "POST") {
    return base::nullopt;
  }

  if (terms[2] != "HTTP/1.1") {
    return base::nullopt;
  }

  return terms;
}

// Return starting index of first HTTP header, -1 on failure.
int LocateStartOfHeaders(base::StringPiece request) {
  auto idx = request.find(kCarriage);
  if (idx == std::string::npos) {
    return -1;
  }

  // Advance to first header and check it exists
  idx += strlen(kCarriage);
  return idx < request.size() ? idx : -1;
}

// Returns a parsed HttpHeader on success, empty Optional on failure.
base::Optional<HttpHeader> ParseHeader(base::StringPiece header) {
  if (header.find(kCarriage) != std::string::npos) {
    return base::nullopt;
  }

  // Parse key
  size_t key_end_index = header.find_first_of(":");
  if (key_end_index == std::string::npos || key_end_index == 0) {
    return base::nullopt;
  }

  const base::StringPiece key = header.substr(0, key_end_index);
  if (!net::HttpUtil::IsValidHeaderName(key)) {
    return base::nullopt;
  }

  // Parse value
  const base::StringPiece remains = header.substr(key_end_index);
  size_t value_begin_index = remains.find_first_not_of(":");
  if (value_begin_index == header.size()) {
    // Empty header value is valid
    return HttpHeader{key.as_string(), ""};
  }

  base::StringPiece value =
      remains.substr(value_begin_index, remains.size() - value_begin_index);
  value = net::HttpUtil::TrimLWS(value);
  if (!net::HttpUtil::IsValidHeaderValue(value)) {
    return base::nullopt;
  }

  return HttpHeader{key.as_string(), value.as_string()};
}

// Returns parsed HTTP request headers on success, empty Optional on
// failure.
// TODO(crbug/894274): Refactor by modifying base::SplitStringIntoKeyValuePairs
base::Optional<std::vector<HttpHeader>> ParseHeaders(
    base::StringPiece headers_slice) {
  auto raw_headers = base::SplitStringPieceUsingSubstr(
      headers_slice, kCarriage, base::TRIM_WHITESPACE,
      base::SPLIT_WANT_NONEMPTY);

  std::vector<HttpHeader> parsed_headers;
  for (auto raw_header : raw_headers) {
    auto header = ParseHeader(raw_header);
    if (!header) {
      return base::nullopt;
    }

    parsed_headers.push_back(header.value());
  }

  return parsed_headers;
}

// Parse IPP request's |ipp_data|
base::Optional<std::vector<uint8_t>> ParseIppData(base::StringPiece ipp_data,
                                                  ipp_op_t opcode) {
  if (!opcode) {
    return base::nullopt;
  }

  // Non-Send-document requests enforced empty |ipp_data|
  if (opcode != IPP_SEND_DOCUMENT) {
    if (ipp_data.empty()) {
      return std::vector<uint8_t>{};
    } else {
      return base::nullopt;
    }
  }

  // Send-document requests require (pdf)ippData
  if (ipp_data.empty()) {
    return base::nullopt;
  }

  // TODO(crbug/894607): Lacking pdf verification

  // Convert and return
  std::vector<uint8_t> parsed_data;
  for (auto c : ipp_data) {
    parsed_data.push_back(static_cast<uint8_t>(c));
  }
  return parsed_data;
}

}  // namespace

IppParser::IppParser(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : service_ref_(std::move(service_ref)) {}

IppParser::~IppParser() = default;

// Checks that |request| is a correctly formatted IPP request, per RFC2910.
// Calls |callback| with a fully parsed IPP request on success, empty on
// failure.
void IppParser::ParseIpp(const std::string& request,
                         ParseIppCallback callback) {
  // StringPiece representing request to help parsing
  // Note: Lifetimes of this StringPiece and |request| match, so this is safe
  // TODO(crbug.com/903561): Investigate mojo std::string -> base::StringPiece
  // type-mapping
  base::StringPiece to_parse(request);

  // Parse request line
  auto request_line = ParseRequestLine(to_parse);
  if (!request_line) {
    return Fail("Failed to parse request line", std::move(callback));
  }

  // Parse headers
  int start_of_headers = LocateStartOfHeaders(to_parse);
  if (start_of_headers < 0) {
    return Fail("Failed to find start of headers", std::move(callback));
  }

  int end_of_headers = net::HttpUtil::LocateEndOfHeaders(
      to_parse.data(), to_parse.size(), start_of_headers);
  if (end_of_headers < 0) {
    return Fail("Failed to find end of headers", std::move(callback));
  }

  const base::StringPiece headers_slice =
      to_parse.substr(start_of_headers, end_of_headers - start_of_headers);
  auto headers = ParseHeaders(headers_slice);
  if (!headers) {
    return Fail("Failed to parse headers", std::move(callback));
  }

  // Parse IPP message
  const base::StringPiece ipp_slice = to_parse.substr(end_of_headers);
  printing::ScopedIppPtr ipp = ReadIppSlice(ipp_slice);
  if (!ipp) {
    return Fail("Failed to read IPP slice", std::move(callback));
  }

  auto ipp_message = ParseIppMessage(ipp.get());
  if (!ipp_message) {
    return Fail("Failed to parse IPP message", std::move(callback));
  }

  // Parse IPP body/payload, if present
  size_t message_len = ippLength(ipp.get());
  if (message_len > ipp_slice.size()) {
    return Fail("Failed to calculate IPP message length", std::move(callback));
  }

  const base::StringPiece ipp_data_slice = ipp_slice.substr(message_len);
  auto ipp_data = ParseIppData(ipp_data_slice, ippGetOperation(ipp.get()));
  if (!ipp_data) {
    return Fail("Failed to parse IPP data", std::move(callback));
  }

  // Marshall response
  mojom::IppRequestPtr parsed_request = mojom::IppRequest::New();

  std::vector<std::string> request_line_terms = request_line.value();
  parsed_request->method = request_line_terms[0];
  parsed_request->endpoint = request_line_terms[1];
  parsed_request->http_version = request_line_terms[2];

  parsed_request->headers = std::move(headers.value());
  parsed_request->ipp = std::move(ipp_message);
  parsed_request->data = std::move(ipp_data.value());

  DVLOG(1) << "Finished parsing IPP request.";
  std::move(callback).Run(mojom::IppParserResult::SUCCESS,
                          std::move(parsed_request));
}

}  // namespace chrome
