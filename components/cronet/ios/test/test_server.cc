// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/ios/test/test_server.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace {

const char kEchoHeaderPath[] = "/EchoHeader?";
const char kSetCookiePath[] = "/SetCookie?";

std::unique_ptr<net::EmbeddedTestServer> g_test_server;

std::unique_ptr<net::test_server::HttpResponse> EchoHeaderInRequest(
    const net::test_server::HttpRequest& request) {
  std::string header_name;
  std::string header_value;
  DCHECK(base::StartsWith(request.relative_url, kEchoHeaderPath,
                          base::CompareCase::INSENSITIVE_ASCII));

  header_name = request.relative_url.substr(strlen(kEchoHeaderPath));
  auto it = request.headers.find(header_name);
  if (it != request.headers.end())
    header_value = it->second;
  auto http_response = base::MakeUnique<net::test_server::BasicHttpResponse>();
  http_response->set_content(header_value);
  return std::move(http_response);
}

std::unique_ptr<net::test_server::HttpResponse> SetAndEchoCookieInResponse(
    const net::test_server::HttpRequest& request) {
  std::string cookie_line;
  DCHECK(base::StartsWith(request.relative_url, kSetCookiePath,
                          base::CompareCase::INSENSITIVE_ASCII));
  cookie_line = request.relative_url.substr(strlen(kSetCookiePath));
  auto http_response = base::MakeUnique<net::test_server::BasicHttpResponse>();
  http_response->set_content(cookie_line);
  http_response->AddCustomHeader("Set-Cookie", cookie_line);
  return std::move(http_response);
}

std::unique_ptr<net::test_server::HttpResponse> CronetTestRequestHandler(
    const net::test_server::HttpRequest& request) {
  if (base::StartsWith(request.relative_url, kEchoHeaderPath,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return EchoHeaderInRequest(request);
  }
  if (base::StartsWith(request.relative_url, kSetCookiePath,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return SetAndEchoCookieInResponse(request);
  }
  return base::MakeUnique<net::test_server::BasicHttpResponse>();
}

}  // namespace

namespace cronet {

/* static */
bool TestServer::Start() {
  DCHECK(!g_test_server.get());
  g_test_server = base::MakeUnique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTP);
  g_test_server->RegisterRequestHandler(base::Bind(&CronetTestRequestHandler));
  CHECK(g_test_server->Start());
  return true;
}

void TestServer::Shutdown() {
  DCHECK(g_test_server.get());
  g_test_server.reset();
}

std::string TestServer::GetEchoHeaderURL(const std::string& header_name) {
  DCHECK(g_test_server);
  return g_test_server->GetURL(kEchoHeaderPath + header_name).spec();
}

std::string TestServer::GetSetCookieURL(const std::string& cookie_line) {
  DCHECK(g_test_server);
  return g_test_server->GetURL(kSetCookiePath + cookie_line).spec();
}

}  // namespace cronet
