// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/nacl/pnacl_header_test.h"

#include "base/bind.h"
#include "base/path_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/nacl/nacl_browsertest_util.h"
#include "content/public/browser/web_contents.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using net::test_server::BasicHttpResponse;
using net::test_server::EmbeddedTestServer;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

PnaclHeaderTest::PnaclHeaderTest() : noncors_loads_(0), cors_loads_(0) {}

PnaclHeaderTest::~PnaclHeaderTest() {}

void PnaclHeaderTest::StartServer() {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // For most requests, just serve files, but register a special test handler
  // that watches for the .pexe fetch also.
  base::FilePath test_data_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&PnaclHeaderTest::WatchForPexeFetch, base::Unretained(this)));
  embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
}

void PnaclHeaderTest::RunLoadTest(const std::string& url,
                                  int expected_noncors,
                                  int expected_cors) {
  StartServer();
  LoadTestMessageHandler handler;
  content::JavascriptTestObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents(),
      &handler);
  ui_test_utils::NavigateToURL(browser(), embedded_test_server()->GetURL(url));
  // Wait until the NMF and pexe are also loaded, not just the HTML.
  // Do this by waiting till the LoadTestMessageHandler responds.
  EXPECT_TRUE(observer.Run()) << handler.error_message();
  EXPECT_TRUE(handler.test_passed()) << "Test failed.";
  EXPECT_EQ(expected_noncors, noncors_loads_);
  EXPECT_EQ(expected_cors, cors_loads_);
}

scoped_ptr<HttpResponse> PnaclHeaderTest::WatchForPexeFetch(
    const HttpRequest& request) {
  // Avoid favicon.ico warning by giving it a dummy icon.
  if (request.relative_url.find("favicon.ico") != std::string::npos) {
    scoped_ptr<BasicHttpResponse> http_response(new BasicHttpResponse());
    http_response->set_code(net::HTTP_OK);
    http_response->set_content("");
    http_response->set_content_type("application/octet-stream");
    return http_response.PassAs<HttpResponse>();
  }

  // Skip other non-pexe files and let ServeFilesFromDirectory handle it.
  GURL absolute_url = embedded_test_server()->GetURL(request.relative_url);
  if (absolute_url.path().find(".pexe") == std::string::npos)
    return scoped_ptr<HttpResponse>();

  // For pexe files, check for the special Accept header.
  // This must match whatever is in:
  // ppapi/native_client/src/trusted/plugin/pnacl_coordinator.cc
  EXPECT_NE(0U, request.headers.count("Accept"));
  std::map<std::string, std::string>::const_iterator it =
      request.headers.find("Accept");
  EXPECT_NE(std::string::npos, it->second.find("application/x-pnacl"));
  EXPECT_NE(std::string::npos, it->second.find("*/*"));

  // Also make sure that other headers like CORS-related headers
  // are preserved when injecting the special Accept header.
  if (absolute_url.path().find("cors") == std::string::npos) {
    EXPECT_EQ(0U, request.headers.count("Origin"));
    noncors_loads_ += 1;
  } else {
    EXPECT_EQ(1U, request.headers.count("Origin"));
    cors_loads_ += 1;
  }

  // After checking the header, just return a 404. We don't need to actually
  // compile and stopping with a 404 is faster.
  scoped_ptr<BasicHttpResponse> http_response(new BasicHttpResponse());
  http_response->set_code(net::HTTP_NOT_FOUND);
  http_response->set_content("PEXE ... not found");
  http_response->set_content_type("application/octet-stream");
  return http_response.PassAs<HttpResponse>();
}

IN_PROC_BROWSER_TEST_F(PnaclHeaderTest, TestHasPnaclHeader) {
  // Load 2 pexes, one same origin and one cross orgin.
  RunLoadTest("/nacl/pnacl_request_header/pnacl_request_header.html", 1, 1);
}
