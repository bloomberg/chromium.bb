// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/ios/test/test_server.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace {

const char kSimplePath[] = "/Simple";
const char kEchoHeaderPath[] = "/EchoHeader?";
const char kSetCookiePath[] = "/SetCookie?";
const char kBigDataPath[] = "/BigData?";
const char kUseEncodingPath[] = "/UseEncoding?";
const char kEchoRequestBodyPath[] = "/EchoRequestBody";

const char kSimpleResponse[] = "The quick brown fox jumps over the lazy dog.";

std::unique_ptr<net::EmbeddedTestServer> g_test_server;
base::LazyInstance<std::string>::Leaky g_big_data_body =
    LAZY_INSTANCE_INITIALIZER;

std::unique_ptr<net::test_server::HttpResponse> SimpleRequest() {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(kSimpleResponse);
  return std::move(http_response);
}

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
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(header_value);
  return std::move(http_response);
}

std::unique_ptr<net::test_server::HttpResponse> UseEncodingInResponse(
    const net::test_server::HttpRequest& request) {
  std::string encoding;
  DCHECK(base::StartsWith(request.relative_url, kUseEncodingPath,
                          base::CompareCase::INSENSITIVE_ASCII));

  encoding = request.relative_url.substr(strlen(kUseEncodingPath));
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  if (!encoding.compare("brotli")) {
    const char quickfoxCompressed[] = {
        0x0b, 0x15, -0x80, 0x54, 0x68, 0x65, 0x20, 0x71, 0x75, 0x69, 0x63, 0x6b,
        0x20, 0x62, 0x72,  0x6f, 0x77, 0x6e, 0x20, 0x66, 0x6f, 0x78, 0x20, 0x6a,
        0x75, 0x6d, 0x70,  0x73, 0x20, 0x6f, 0x76, 0x65, 0x72, 0x20, 0x74, 0x68,
        0x65, 0x20, 0x6c,  0x61, 0x7a, 0x79, 0x20, 0x64, 0x6f, 0x67, 0x03};
    std::string quickfoxCompressedStr(quickfoxCompressed,
                                      sizeof(quickfoxCompressed));
    http_response->set_code(net::HTTP_OK);
    http_response->set_content(quickfoxCompressedStr);
    http_response->AddCustomHeader(std::string("content-encoding"),
                                   std::string("br"));
  }
  return std::move(http_response);
}

std::unique_ptr<net::test_server::HttpResponse> EchoResponseBody(
    const net::test_server::HttpRequest& request) {
  DCHECK(base::StartsWith(request.relative_url, kEchoRequestBodyPath,
                          base::CompareCase::INSENSITIVE_ASCII));
  std::string request_content = request.content;
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(request_content);
  return http_response;
}

std::unique_ptr<net::test_server::HttpResponse> ReturnBigDataInResponse(
    const net::test_server::HttpRequest& request) {
  DCHECK(base::StartsWith(request.relative_url, kBigDataPath,
                          base::CompareCase::INSENSITIVE_ASCII));
  std::string data_size_str = request.relative_url.substr(strlen(kBigDataPath));
  int64_t data_size;
  CHECK(base::StringToInt64(base::StringPiece(data_size_str), &data_size));
  CHECK(data_size == static_cast<int64_t>(g_big_data_body.Get().size()));
  return std::make_unique<net::test_server::RawHttpResponse>(
      std::string(), g_big_data_body.Get());
}

std::unique_ptr<net::test_server::HttpResponse> SetAndEchoCookieInResponse(
    const net::test_server::HttpRequest& request) {
  std::string cookie_line;
  DCHECK(base::StartsWith(request.relative_url, kSetCookiePath,
                          base::CompareCase::INSENSITIVE_ASCII));
  cookie_line = request.relative_url.substr(strlen(kSetCookiePath));
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(cookie_line);
  http_response->AddCustomHeader("Set-Cookie", cookie_line);
  return std::move(http_response);
}

std::unique_ptr<net::test_server::HttpResponse> CronetTestRequestHandler(
    const net::test_server::HttpRequest& request) {
  if (base::StartsWith(request.relative_url, kSimplePath,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return SimpleRequest();
  }
  if (base::StartsWith(request.relative_url, kEchoHeaderPath,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return EchoHeaderInRequest(request);
  }
  if (base::StartsWith(request.relative_url, kSetCookiePath,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return SetAndEchoCookieInResponse(request);
  }
  if (base::StartsWith(request.relative_url, kBigDataPath,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return ReturnBigDataInResponse(request);
  }
  if (base::StartsWith(request.relative_url, kUseEncodingPath,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return UseEncodingInResponse(request);
  }
  if (base::StartsWith(request.relative_url, kEchoRequestBodyPath,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return EchoResponseBody(request);
  }
  return std::make_unique<net::test_server::BasicHttpResponse>();
}

}  // namespace

namespace cronet {

/* static */
bool TestServer::Start() {
  DCHECK(!g_test_server.get());
  g_test_server = std::make_unique<net::EmbeddedTestServer>(
      net::EmbeddedTestServer::TYPE_HTTP);
  g_test_server->RegisterRequestHandler(base::Bind(&CronetTestRequestHandler));
  CHECK(g_test_server->Start());
  return true;
}

void TestServer::Shutdown() {
  DCHECK(g_test_server.get());
  g_test_server.reset();
}

std::string TestServer::GetSimpleURL() {
  DCHECK(g_test_server);
  return g_test_server->GetURL(kSimplePath).spec();
}

std::string TestServer::GetEchoHeaderURL(const std::string& header_name) {
  DCHECK(g_test_server);
  return g_test_server->GetURL(kEchoHeaderPath + header_name).spec();
}

std::string TestServer::GetUseEncodingURL(const std::string& header_name) {
  DCHECK(g_test_server);
  return g_test_server->GetURL(kUseEncodingPath + header_name).spec();
}

std::string TestServer::GetSetCookieURL(const std::string& cookie_line) {
  DCHECK(g_test_server);
  return g_test_server->GetURL(kSetCookiePath + cookie_line).spec();
}

std::string TestServer::EchoRequestBodyURL() {
  DCHECK(g_test_server);
  return g_test_server->GetURL(kEchoRequestBodyPath).spec();
}

std::string TestServer::PrepareBigDataURL(long data_size) {
  DCHECK(g_test_server);
  DCHECK(g_big_data_body.Get().empty());
  // Response line with headers.
  std::string response_builder;
  base::StringAppendF(&response_builder, "HTTP/1.1 200 OK\r\n");
  base::StringAppendF(&response_builder, "Content-Length: %" PRIuS "\r\n",
                      data_size);
  base::StringAppendF(&response_builder, "\r\n");
  response_builder += std::string(data_size, 'c');
  g_big_data_body.Get() = response_builder;
  return g_test_server
      ->GetURL(kBigDataPath + base::Int64ToString(response_builder.size()))
      .spec();
}

void TestServer::ReleaseBigDataURL() {
  DCHECK(!g_big_data_body.Get().empty());
  g_big_data_body.Get() = std::string();
}

}  // namespace cronet
