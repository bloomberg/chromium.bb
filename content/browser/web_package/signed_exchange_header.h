// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_HEADER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_HEADER_H_

#include <map>
#include <string>

#include "base/containers/span.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "content/browser/web_package/signed_exchange_header_parser.h"
#include "content/common/content_export.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

namespace content {

// SignedExchangeHeader contains all information captured in signed exchange
// envelope but the payload.
// https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html
class CONTENT_EXPORT SignedExchangeHeader {
 public:
  static constexpr size_t kEncodedHeaderLengthInBytes = 3;
  // Parse big-endian encoded length of the following CBOR-encoded
  // signed exchange header.
  // Note: |input| must be pointing to a valid memory address that has at least
  // |kEncodedHeaderLengthInBytes|.
  static size_t ParseHeadersLength(base::span<const uint8_t> input);

  // Parse headers from the new serialization format currently being discussed.
  // 1. The first 3 bytes of the content represents the length of the CBOR
  // encoded section, encoded in network byte (big-endian) order. 2. Then,
  // immediately follows a CBOR-encoded array containing 2 elements: (This is
  // derived from the section 5 of the old spec) - a map of request header field
  // names to values, encoded as byte strings, with ":method", and ":url" pseudo
  // header fields - a map from response header field names to values, encoded
  // as byte strings, with a ":status" pseudo-header field containing the status
  // code (encoded as 3 ASCII letter byte string) 3. Then, immediately follows
  // the response body, encoded in MI.
  // TODO(kouhei): Replace above with spec reference when we actually have spec
  // text.
  //
  // This also performs the step 3 and 4 of "Cross-origin trust" validation.
  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#rfc.section.4
  static base::Optional<SignedExchangeHeader> Parse(
      base::span<const uint8_t> input);
  SignedExchangeHeader();
  SignedExchangeHeader(const SignedExchangeHeader&);
  SignedExchangeHeader(SignedExchangeHeader&&);
  SignedExchangeHeader& operator=(SignedExchangeHeader&&);
  ~SignedExchangeHeader();

  void AddResponseHeader(base::StringPiece name, base::StringPiece value);
  scoped_refptr<net::HttpResponseHeaders> BuildHttpResponseHeaders() const;

  const GURL& request_url() const { return request_url_; };
  void set_request_url(GURL url) { request_url_ = std::move(url); }

  const std::string& request_method() const { return request_method_; }
  void set_request_method(base::StringPiece s) {
    s.CopyToString(&request_method_);
  }

  net::HttpStatusCode response_code() const { return response_code_; }
  void set_response_code(net::HttpStatusCode c) { response_code_ = c; }

  const std::map<std::string, std::string>& response_headers() const {
    return response_headers_;
  }

  const SignedExchangeHeaderParser::Signature& signature() const {
    return signature_;
  }
  void SetSignatureForTesting(
      const SignedExchangeHeaderParser::Signature& sig) {
    signature_ = sig;
  }

 private:
  GURL request_url_;
  std::string request_method_;

  net::HttpStatusCode response_code_;
  std::map<std::string, std::string> response_headers_;
  SignedExchangeHeaderParser::Signature signature_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_HEADER_H_
