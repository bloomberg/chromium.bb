// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_ipp_parser/ipp_parser.h"

#include <cups/cups.h>
#include <memory>
#include <utility>

#include "base/optional.h"
#include "chrome/services/cups_ipp_parser/public/cpp/ipp_converter.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/http/http_util.h"

namespace chrome {
namespace {

using ipp_converter::HttpHeader;
using ipp_converter::kCarriage;
using ipp_converter::kIppSentinel;

// Log debugging error and send empty response, signalling error.
void Fail(const std::string& error_log, IppParser::ParseIppCallback cb) {
  DVLOG(1) << "IPP Parser Error: " << error_log;
  std::move(cb).Run(mojom::IppParserResult::FAILURE, nullptr);
  return;
}

// Returns the starting index of the request-line-delimiter, -1 on failure.
int LocateEndOfRequestLine(base::StringPiece request) {
  auto end_of_request_line = request.find(kCarriage);
  if (end_of_request_line == std::string::npos) {
    return -1;
  }

  return end_of_request_line;
}

// Returns the starting index of the first HTTP header, -1 on failure.
int LocateStartOfHeaders(base::StringPiece request) {
  auto idx = request.find(kCarriage);
  if (idx == base::StringPiece::npos) {
    return -1;
  }

  // Advance to first header and check it exists
  idx += strlen(kCarriage);
  return idx < request.size() ? idx : -1;
}

// Returns the starting index of the end-of-headers-delimiter, -1 on failure.
int LocateEndOfHeaders(base::StringPiece request) {
  auto idx = net::HttpUtil::LocateEndOfHeaders(request.data(), request.size());
  if (idx < 0) {
    return -1;
  }

  // Back up to the start of the delimiter.
  // Note: The end-of-http-headers delimiter is 2 back-to-back carriage returns.
  const size_t end_of_headers_delimiter_size = 2 * strlen(kCarriage);
  return idx - end_of_headers_delimiter_size;
}

// Returns the starting index of the IPP message, -1 on failure.
int LocateStartOfIppMessage(base::StringPiece request) {
  return net::HttpUtil::LocateEndOfHeaders(request.data(), request.size());
}

// Return the starting index of the IPP data/payload(pdf),
// Returns |request|.size() on empty IppData and -1 on failure.
int LocateStartOfIppData(base::StringPiece request) {
  int end_of_headers = LocateEndOfHeaders(request);
  if (end_of_headers < 0) {
    return -1;
  }

  auto idx = request.find(kIppSentinel, end_of_headers);
  if (idx == base::StringPiece::npos) {
    return -1;
  }

  // Advance to start and check existence or end of request.
  idx += strlen(kIppSentinel);
  return idx <= request.size() ? idx : -1;
}

base::Optional<std::vector<std::string>> ExtractRequestLine(
    base::StringPiece request) {
  int end_of_request_line = LocateEndOfRequestLine(request);
  if (end_of_request_line < 0) {
    return base::nullopt;
  }

  const base::StringPiece request_line_slice =
      request.substr(0, end_of_request_line);
  return ipp_converter::ParseRequestLine(request_line_slice);
}

base::Optional<std::vector<HttpHeader>> ExtractHeaders(
    base::StringPiece request) {
  int start_of_headers = LocateStartOfHeaders(request);
  if (start_of_headers < 0) {
    return base::nullopt;
  }

  int end_of_headers = LocateEndOfHeaders(request);
  if (end_of_headers < 0) {
    return base::nullopt;
  }

  const base::StringPiece headers_slice =
      request.substr(start_of_headers, end_of_headers - start_of_headers);
  return ipp_converter::ParseHeaders(headers_slice);
}

mojom::IppMessagePtr ExtractIppMessage(base::StringPiece request) {
  int start_of_ipp_message = LocateStartOfIppMessage(request);
  if (start_of_ipp_message < 0) {
    return nullptr;
  }

  std::vector<uint8_t> ipp_slice =
      ipp_converter::ConvertToByteBuffer(request.substr(start_of_ipp_message));
  printing::ScopedIppPtr ipp = ipp_converter::ParseIppMessage(ipp_slice);
  if (!ipp) {
    return nullptr;
  }

  return ipp_converter::ConvertIppToMojo(ipp.release());
}

// Parse IPP request's |ipp_data|
base::Optional<std::vector<uint8_t>> ExtractIppData(base::StringPiece request) {
  size_t start_of_ipp_data = LocateStartOfIppData(request);
  if (start_of_ipp_data < 0) {
    return base::nullopt;
  }

  // Subtlety: Correctly generates empty buffers for requests without ipp_data.
  const base::StringPiece ipp_data_slice = request.substr(start_of_ipp_data);
  return ipp_converter::ConvertToByteBuffer(ipp_data_slice);
}

}  // namespace

IppParser::IppParser(
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : service_ref_(std::move(service_ref)) {}

IppParser::~IppParser() = default;

// Checks that |to_parse| is a correctly formatted IPP request, per RFC2910.
// Calls |callback| with a fully parsed IPP request on success, empty on
// failure.
void IppParser::ParseIpp(const std::string& to_parse,
                         ParseIppCallback callback) {
  // Parse Request line
  auto request_line = ExtractRequestLine(to_parse);
  if (!request_line) {
    return Fail("Failed to parse request line", std::move(callback));
  }

  // Parse Headers
  auto headers = ExtractHeaders(to_parse);
  if (!headers) {
    return Fail("Failed to parse headers", std::move(callback));
  }

  // Parse IPP message
  auto ipp_message = ExtractIppMessage(to_parse);
  if (!ipp_message) {
    return Fail("Failed to parse IPP message", std::move(callback));
  }

  // Parse IPP data
  auto ipp_data = ExtractIppData(to_parse);
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
