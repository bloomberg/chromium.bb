// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/testing/embedded_test_server_handlers.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "net/base/escape.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace testing {

std::unique_ptr<net::test_server::HttpResponse> HandleIFrame(
    const net::test_server::HttpRequest& request) {
  GURL request_url = request.GetURL();
  std::string iframe_src = net::UnescapeBinaryURLComponent(request_url.query());

  // Escape iframe src.
  GURL iframe_url(iframe_src);
  if (iframe_url.is_valid()) {
    iframe_src = net::EscapeForHTML(iframe_url.spec());
  }

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_content_type("text/html");
  http_response->set_content(base::StringPrintf(
      "<html><head></head><body><iframe src='%s'></iframe></body></html>",
      iframe_src.c_str()));
  return std::move(http_response);
}

std::unique_ptr<net::test_server::HttpResponse> HandleEchoQueryOrCloseSocket(
    const bool& responds_with_content,
    const net::test_server::HttpRequest& request) {
  if (!responds_with_content) {
    return std::make_unique<net::test_server::RawHttpResponse>(
        /*headers=*/"", /*contents=*/"");
  }
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/html");
  response->set_content(request.GetURL().query());
  return std::move(response);
}

}  // namespace testing
