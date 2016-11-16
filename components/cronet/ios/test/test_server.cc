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

std::unique_ptr<net::EmbeddedTestServer> g_test_server;

std::unique_ptr<net::test_server::HttpResponse> EchoHeaderInRequest(
    const net::test_server::HttpRequest& request) {
  std::string header_name;
  std::string header_value;
  if (base::StartsWith(request.relative_url, kEchoHeaderPath,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    header_name = request.relative_url.substr(strlen(kEchoHeaderPath));
  }
  auto it = request.headers.find(header_name);
  if (it != request.headers.end())
    header_value = it->second;
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse());
  http_response->set_content(header_value);
  return std::move(http_response);
}

}  // namespace

namespace cronet {

/* static */
bool TestServer::Start() {
  DCHECK(!g_test_server.get());
  g_test_server = base::MakeUnique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTP);
  g_test_server->RegisterRequestHandler(base::Bind(&EchoHeaderInRequest));
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

}  // namespace cronet
