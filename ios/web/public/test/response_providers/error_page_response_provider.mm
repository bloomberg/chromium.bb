// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/response_providers/error_page_response_provider.h"

#include "base/logging.h"
#import "ios/web/public/test/http_server.h"
#include "net/http/http_status_code.h"

// static
GURL ErrorPageResponseProvider::GetDnsFailureUrl() {
  return GURL("http://mock/bad");
}

// static
GURL ErrorPageResponseProvider::GetRedirectToDnsFailureUrl() {
  return web::test::HttpServer::MakeUrl("http://mock/redirect");
}

// Returns true for |RedirectToDnsFailureUrl|.
bool ErrorPageResponseProvider::CanHandleRequest(const Request& request) {
  return (HtmlResponseProvider::CanHandleRequest(request) ||
          request.url ==
              ErrorPageResponseProvider::GetRedirectToDnsFailureUrl());
}

void ErrorPageResponseProvider::GetResponseHeadersAndBody(
    const Request& request,
    scoped_refptr<net::HttpResponseHeaders>* headers,
    std::string* response_body) {
  if (HtmlResponseProvider::CanHandleRequest(request)) {
    HtmlResponseProvider::GetResponseHeadersAndBody(request, headers,
                                                    response_body);
  } else if (request.url == GetRedirectToDnsFailureUrl()) {
    *headers = web::ResponseProvider::GetRedirectResponseHeaders(
        ErrorPageResponseProvider::GetDnsFailureUrl().spec(), net::HTTP_FOUND);
  } else {
    NOTREACHED();
  }
}
