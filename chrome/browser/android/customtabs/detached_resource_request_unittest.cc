// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "chrome/browser/android/customtabs/detached_resource_request.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace customtabs {

namespace {
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using net::test_server::RequestQuery;

constexpr const char kSetCookieAndRedirect[] = "/set-cookie-and-redirect";
constexpr const char kSetCookieAndNoContent[] = "/set-cookie-and-no-content";
constexpr const char kHttpNoContent[] = "/nocontent";
constexpr const char kEchoTitle[] = "/echotitle";
constexpr const char kCookieKey[] = "cookie";
constexpr const char kUrlKey[] = "url";

// /set-cookie-and-redirect?cookie=bla&url=https://redictected-url
// Sets a cookies, then responds with HTTP code 302.
std::unique_ptr<HttpResponse> SetCookieAndRedirect(const HttpRequest& request) {
  const auto& url = request.GetURL();
  if (url.path() != kSetCookieAndRedirect || !url.has_query())
    return nullptr;

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  RequestQuery query = net::test_server::ParseQuery(url);
  for (const char* key : {kCookieKey, kUrlKey}) {
    if (query.find(key) == query.end())
      return nullptr;
  }
  std::string cookie = query[kCookieKey][0];
  std::string destination = query[kUrlKey][0];

  response->AddCustomHeader("Set-Cookie", cookie);
  response->AddCustomHeader("Location", destination);
  response->set_code(net::HTTP_FOUND);
  response->set_content_type("text/html");
  response->set_content(base::StringPrintf(
      "<html><head></head><body>Redirecting to %s</body></html>",
      destination.c_str()));
  return response;
}

// /set-cookie-and-no-content?cookie=bla
// Sets a cookies, and replies with HTTP code 204.
std::unique_ptr<HttpResponse> SetCookieAndNoContent(
    const HttpRequest& request) {
  const auto& url = request.GetURL();
  if (url.path() != kSetCookieAndNoContent || !url.has_query())
    return nullptr;

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  RequestQuery query = net::test_server::ParseQuery(url);
  if (query.find(kCookieKey) == query.end())
    return nullptr;
  std::string cookie = query[kCookieKey][0];

  response->AddCustomHeader("Set-Cookie", cookie);
  response->set_code(net::HTTP_NO_CONTENT);
  return response;
}

// Waits for |expected_requests| requests to |path|, then reports the headers
// in |headers| and calls |closure|.
// Output parameters can be nullptr.
void WatchPathAndReportHeaders(const std::string& path,
                               int* expected_requests,
                               HttpRequest::HeaderMap* headers,
                               base::OnceClosure closure,
                               const HttpRequest& request) {
  if (request.GetURL().path() != path)
    return;
  if (expected_requests && --*expected_requests)
    return;
  if (headers)
    *headers = request.headers;
  std::move(closure).Run();
}

}  // namespace

class DetachedResourceRequestTest : public ::testing::Test {
 public:
  DetachedResourceRequestTest()
      : thread_bundle_(content::TestBrowserThreadBundle::REAL_IO_THREAD) {}
  ~DetachedResourceRequestTest() override = default;

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    test_server_ = std::make_unique<net::EmbeddedTestServer>();
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&SetCookieAndRedirect));
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&SetCookieAndNoContent));
    embedded_test_server()->AddDefaultHandlers(
        base::FilePath("chrome/test/data"));
  }

  void TearDown() override {
    profile_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

 protected:
  net::EmbeddedTestServer* embedded_test_server() const {
    return test_server_.get();
  }

  content::BrowserContext* browser_context() const { return profile_.get(); }

  void SetAndCheckCookieWithRedirect(bool third_party) {
    base::RunLoop first_request_waiter;
    base::RunLoop second_request_waiter;

    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &WatchPathAndReportHeaders, kSetCookieAndRedirect, nullptr, nullptr,
        first_request_waiter.QuitClosure()));
    embedded_test_server()->RegisterRequestMonitor(
        base::BindRepeating(&WatchPathAndReportHeaders, kHttpNoContent, nullptr,
                            nullptr, second_request_waiter.QuitClosure()));
    ASSERT_TRUE(embedded_test_server()->Start());

    GURL redirected_url(embedded_test_server()->GetURL(kHttpNoContent));
    std::string relative_url =
        base::StringPrintf("%s?%s=%s&%s=%s", kSetCookieAndRedirect, kCookieKey,
                           "acookie", kUrlKey, redirected_url.spec().c_str());

    GURL url(embedded_test_server()->GetURL(relative_url));
    GURL site_for_cookies = third_party ? GURL("http://cats.google.com")
                                        : embedded_test_server()->base_url();

    std::string cookie = content::GetCookies(browser_context(), url);
    ASSERT_EQ("", cookie);

    DetachedResourceRequest::CreateAndStart(browser_context(), url,
                                            site_for_cookies);
    first_request_waiter.Run();
    second_request_waiter.Run();

    cookie = content::GetCookies(browser_context(), url);
    ASSERT_EQ("acookie", cookie);
  }

 private:
  std::unique_ptr<TestingProfile> profile_;
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<net::EmbeddedTestServer> test_server_;
};

TEST_F(DetachedResourceRequestTest, Simple) {
  base::RunLoop request_completion_waiter;
  base::RunLoop server_request_waiter;
  HttpRequest::HeaderMap headers;

  embedded_test_server()->RegisterRequestMonitor(
      base::BindRepeating(&WatchPathAndReportHeaders, kEchoTitle, nullptr,
                          &headers, server_request_waiter.QuitClosure()));
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kEchoTitle));
  GURL site_for_cookies("http://cats.google.com/");

  DetachedResourceRequest::CreateAndStart(
      browser_context(), url, site_for_cookies,
      base::BindOnce(
          [](base::OnceClosure closure, bool success) {
            EXPECT_TRUE(success);
            std::move(closure).Run();
          },
          request_completion_waiter.QuitClosure()));
  server_request_waiter.Run();
  EXPECT_EQ(site_for_cookies.spec(), headers["referer"]);
  request_completion_waiter.Run();
}

TEST_F(DetachedResourceRequestTest, SimpleFailure) {
  base::RunLoop request_waiter;
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/unknown-url"));
  GURL site_for_cookies(embedded_test_server()->base_url());

  DetachedResourceRequest::CreateAndStart(
      browser_context(), url, site_for_cookies,
      base::BindOnce(
          [](base::OnceClosure closure, bool success) {
            EXPECT_FALSE(success);
            std::move(closure).Run();
          },
          request_waiter.QuitClosure()));
  request_waiter.Run();
}

TEST_F(DetachedResourceRequestTest, MultipleRequests) {
  base::RunLoop request_waiter;
  int expected_requests = 2;
  HttpRequest::HeaderMap headers;

  embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
      &WatchPathAndReportHeaders, kEchoTitle, &expected_requests, &headers,
      request_waiter.QuitClosure()));
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL(kEchoTitle));
  GURL site_for_cookies(embedded_test_server()->base_url());

  // No request coalescing, and no cache hit for a no-cache resource.
  for (int i = 0; i < 2; ++i) {
    DetachedResourceRequest::CreateAndStart(browser_context(), url,
                                            site_for_cookies);
  }
  request_waiter.Run();
  EXPECT_EQ(site_for_cookies.spec(), headers["referer"]);
}

TEST_F(DetachedResourceRequestTest, NoReferrerWhenDowngrade) {
  base::RunLoop request_waiter;
  HttpRequest::HeaderMap headers;

  embedded_test_server()->RegisterRequestMonitor(
      base::BindRepeating(&WatchPathAndReportHeaders, kEchoTitle, nullptr,
                          &headers, request_waiter.QuitClosure()));
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL(kEchoTitle));
  // Downgrade, as the server is over HTTP.
  GURL site_for_cookies("https://cats.google.com");

  DetachedResourceRequest::CreateAndStart(browser_context(), url,
                                          site_for_cookies);
  request_waiter.Run();
  EXPECT_EQ("", headers["referer"]);
}

TEST_F(DetachedResourceRequestTest, FollowRedirect) {
  base::RunLoop first_request_waiter;
  base::RunLoop second_request_waiter;

  std::string initial_relative_url =
      std::string(kSetCookieAndRedirect) + "?cookie=acookie&url=";

  embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
      &WatchPathAndReportHeaders, kSetCookieAndRedirect, nullptr, nullptr,
      first_request_waiter.QuitClosure()));
  embedded_test_server()->RegisterRequestMonitor(
      base::BindRepeating(&WatchPathAndReportHeaders, kHttpNoContent, nullptr,
                          nullptr, second_request_waiter.QuitClosure()));
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL redirected_url(embedded_test_server()->GetURL(kHttpNoContent));
  GURL url(embedded_test_server()->GetURL(initial_relative_url +
                                          redirected_url.spec()));
  GURL site_for_cookies(embedded_test_server()->base_url());

  DetachedResourceRequest::CreateAndStart(browser_context(), url,
                                          site_for_cookies);
  first_request_waiter.Run();
  second_request_waiter.Run();
}

TEST_F(DetachedResourceRequestTest, CanSetCookie) {
  SetAndCheckCookieWithRedirect(false);
}

TEST_F(DetachedResourceRequestTest, CanSetThirdPartyCookie) {
  SetAndCheckCookieWithRedirect(true);
}

TEST_F(DetachedResourceRequestTest, NoContentCanSetCookie) {
  base::RunLoop request_completion_waiter;
  ASSERT_TRUE(embedded_test_server()->Start());

  auto relative_url = base::StringPrintf("%s?%s=%s", kSetCookieAndNoContent,
                                         kCookieKey, "acookie");
  GURL url(embedded_test_server()->GetURL(relative_url));
  GURL site_for_cookies("http://cats.google.com/");

  std::string cookie = content::GetCookies(browser_context(), url);
  ASSERT_EQ("", cookie);

  DetachedResourceRequest::CreateAndStart(
      browser_context(), url, site_for_cookies,
      base::BindOnce(
          [](base::OnceClosure closure, bool success) {
            EXPECT_TRUE(success);
            std::move(closure).Run();
          },
          request_completion_waiter.QuitClosure()));

  request_completion_waiter.Run();
  cookie = content::GetCookies(browser_context(), url);
  ASSERT_EQ("acookie", cookie);
}

}  // namespace customtabs
