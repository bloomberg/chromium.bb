// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/async_resource_handler.h"

#include <string>

#include "base/strings/string_util.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const char kPostPath[] = "/post";
const char kRedirectPostPath[] = "/redirect";
const size_t kPayloadSize = 28697814;  // 2*3^15

scoped_ptr<net::test_server::HttpResponse> HandlePostAndRedirectURLs(
  const std::string& request_path,
  const net::test_server::HttpRequest& request) {
  scoped_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse());
  if (base::StartsWith(request.relative_url, kRedirectPostPath,
                       base::CompareCase::SENSITIVE)) {
    http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
    http_response->AddCustomHeader("Location", kPostPath);
    EXPECT_EQ(request.content.length(), kPayloadSize);;
    return http_response.Pass();
  } else if(base::StartsWith(request.relative_url, kPostPath,
                             base::CompareCase::SENSITIVE)) {
    http_response->set_content("hello");
    http_response->set_content_type("text/plain");
    EXPECT_EQ(request.content.length(), kPayloadSize);
    return http_response.Pass();
  } else {
    return scoped_ptr<net::test_server::HttpResponse>();
  }
}

}  // namespace

class AsyncResourceHandlerBrowserTest : public ContentBrowserTest {
};

IN_PROC_BROWSER_TEST_F(AsyncResourceHandlerBrowserTest, UploadProgress) {
  net::test_server::EmbeddedTestServer* test_server = embedded_test_server();
  ASSERT_TRUE(test_server->InitializeAndWaitUntilReady());
  test_server->RegisterRequestHandler(
      base::Bind(&HandlePostAndRedirectURLs, kPostPath));

  NavigateToURL(shell(),
                test_server->GetURL("/loader/async_resource_handler.html"));

  std::string js_result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell()->web_contents(),
      "WaitForAsyncXHR('/post')",
      &js_result));
  EXPECT_EQ(js_result, "success");
}

IN_PROC_BROWSER_TEST_F(AsyncResourceHandlerBrowserTest,
                       UploadProgressRedirect) {
  net::test_server::EmbeddedTestServer* test_server = embedded_test_server();
  ASSERT_TRUE(test_server->InitializeAndWaitUntilReady());
  test_server->RegisterRequestHandler(
      base::Bind(&HandlePostAndRedirectURLs, kRedirectPostPath));

  NavigateToURL(shell(),
                test_server->GetURL("/loader/async_resource_handler.html"));

  std::string js_result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell()->web_contents(),
      "WaitForAsyncXHR('/redirect')",
      &js_result));
  EXPECT_EQ(js_result, "success");
}

}  // namespace content
