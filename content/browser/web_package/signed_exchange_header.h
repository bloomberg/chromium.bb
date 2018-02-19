// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_HEADER_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_HEADER_H_

#include <map>
#include <string>

#include "base/strings/string_piece.h"
#include "content/common/content_export.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

namespace content {

// SignedExchangeHeader contains all information captured in signed exchange
// envelope but the payload.
// https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html
class CONTENT_EXPORT SignedExchangeHeader {
 public:
  SignedExchangeHeader();
  ~SignedExchangeHeader();

  void AddResponseHeader(base::StringPiece name, base::StringPiece value);

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

 private:
  GURL request_url_;
  std::string request_method_;

  net::HttpStatusCode response_code_;
  std::map<std::string, std::string> response_headers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_HEADER_H_
