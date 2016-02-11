// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/response_providers/response_provider.h"

#include "base/strings/stringprintf.h"
#include "net/http/http_response_headers.h"

namespace web {

ResponseProvider::Request::Request(const GURL& url,
                                   const std::string& method,
                                   const std::string& body,
                                   const net::HttpRequestHeaders& headers)
    : url(url), method(method), body(body), headers(headers) {}

ResponseProvider::Request::~Request() {}

ResponseProvider::ResponseProvider() {}

// static
scoped_refptr<net::HttpResponseHeaders> ResponseProvider::GetResponseHeaders(
    const std::string& content_type,
    net::HttpStatusCode response_code) {
  scoped_refptr<net::HttpResponseHeaders> result(
      new net::HttpResponseHeaders(""));
  const std::string reason_phrase(net::GetHttpReasonPhrase(response_code));
  const std::string status_line = base::StringPrintf(
      "HTTP/1.1 %i %s", static_cast<int>(response_code), reason_phrase.c_str());
  result->ReplaceStatusLine(status_line);
  const std::string content_type_header =
      base::StringPrintf("Content-type: %s", content_type.c_str());
  result->AddHeader(content_type_header);
  return result;
}

// static
scoped_refptr<net::HttpResponseHeaders> ResponseProvider::GetResponseHeaders(
    const std::string& content_type) {
  return GetResponseHeaders(content_type, net::HTTP_OK);
}

// static
scoped_refptr<net::HttpResponseHeaders>
ResponseProvider::GetDefaultResponseHeaders() {
  return GetResponseHeaders("text/html", net::HTTP_OK);
}

// static
scoped_refptr<net::HttpResponseHeaders>
ResponseProvider::GetRedirectResponseHeaders(
    const std::string& destination,
    const net::HttpStatusCode& http_status) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      GetResponseHeaders("text/html", http_status));
  headers->AddHeader("Location: " + destination);
  return headers;
}

}  // namespace web
