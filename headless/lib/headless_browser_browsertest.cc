// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "content/public/test/browser_test.h"
#include "headless/public/domains/page.h"
#include "headless/public/domains/runtime.h"
#include "headless/public/domains/types.h"
#include "headless/public/headless_browser.h"
#include "headless/public/headless_browser_context.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/headless_devtools_target.h"
#include "headless/public/headless_web_contents.h"
#include "headless/test/headless_browser_test.h"
#include "net/base/io_buffer.h"
#include "net/http/http_response_headers.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/url_request/url_request_job.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace headless {
namespace {

class TestURLRequestJob : public net::URLRequestJob {
 public:
  TestURLRequestJob(net::URLRequest* request,
                    net::NetworkDelegate* network_delegate,
                    const std::string& body);
  ~TestURLRequestJob() override {}

  // net::URLRequestJob implementation:
  void Start() override;
  void GetResponseInfo(net::HttpResponseInfo* info) override;
  int ReadRawData(net::IOBuffer* buf, int buf_size) override;

 private:
  scoped_refptr<net::StringIOBuffer> body_;
  scoped_refptr<net::DrainableIOBuffer> src_buf_;

  DISALLOW_COPY_AND_ASSIGN(TestURLRequestJob);
};

TestURLRequestJob::TestURLRequestJob(net::URLRequest* request,
                                     net::NetworkDelegate* network_delegate,
                                     const std::string& body)
    : net::URLRequestJob(request, network_delegate),
      body_(new net::StringIOBuffer(body)),
      src_buf_(new net::DrainableIOBuffer(body_.get(), body_->size())) {}

void TestURLRequestJob::Start() {
  NotifyHeadersComplete();
}

void TestURLRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  info->headers =
      new net::HttpResponseHeaders("Content-Type: text/html\r\n\r\n");
}

int TestURLRequestJob::ReadRawData(net::IOBuffer* buf, int buf_size) {
  scoped_refptr<net::DrainableIOBuffer> dest_buf(
      new net::DrainableIOBuffer(buf, buf_size));
  while (src_buf_->BytesRemaining() > 0 && dest_buf->BytesRemaining() > 0) {
    *dest_buf->data() = *src_buf_->data();
    src_buf_->DidConsume(1);
    dest_buf->DidConsume(1);
  }
  return dest_buf->BytesConsumed();
}

class TestProtocolHandler : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  TestProtocolHandler(const std::string& body);
  ~TestProtocolHandler() override {}

  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

 private:
  std::string body_;

  DISALLOW_COPY_AND_ASSIGN(TestProtocolHandler);
};

TestProtocolHandler::TestProtocolHandler(const std::string& body)
    : body_(body) {}

net::URLRequestJob* TestProtocolHandler::MaybeCreateJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  return new TestURLRequestJob(request, network_delegate, body_);
}

}  // namespace

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, CreateAndDestroyWebContents) {
  HeadlessWebContents* web_contents =
      browser()->CreateWebContentsBuilder().Build();
  EXPECT_TRUE(web_contents);

  EXPECT_EQ(static_cast<size_t>(1), browser()->GetAllWebContents().size());
  EXPECT_EQ(web_contents, browser()->GetAllWebContents()[0]);
  // TODO(skyostil): Verify viewport dimensions once we can.
  web_contents->Close();

  EXPECT_TRUE(browser()->GetAllWebContents().empty());
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, CreateWithBadURL) {
  GURL bad_url("not_valid");
  HeadlessWebContents* web_contents =
      browser()->CreateWebContentsBuilder().SetInitialURL(bad_url).Build();
  EXPECT_FALSE(web_contents);
  EXPECT_TRUE(browser()->GetAllWebContents().empty());
}

class HeadlessBrowserTestWithProxy : public HeadlessBrowserTest {
 public:
  HeadlessBrowserTestWithProxy()
      : proxy_server_(net::SpawnedTestServer::TYPE_HTTP,
                      net::SpawnedTestServer::kLocalhost,
                      base::FilePath(FILE_PATH_LITERAL("headless/test/data"))) {
  }

  void SetUp() override {
    ASSERT_TRUE(proxy_server_.Start());
    HeadlessBrowserTest::SetUp();
  }

  void TearDown() override {
    proxy_server_.Stop();
    HeadlessBrowserTest::TearDown();
  }

  net::SpawnedTestServer* proxy_server() { return &proxy_server_; }

 private:
  net::SpawnedTestServer proxy_server_;
};

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTestWithProxy, SetProxyServer) {
  HeadlessBrowser::Options::Builder builder;
  builder.SetProxyServer(proxy_server()->host_port_pair());
  SetBrowserOptions(builder.Build());

  // Load a page which doesn't actually exist, but for which the our proxy
  // returns valid content anyway.
  //
  // TODO(altimin): Currently this construction does not serve hello.html
  // from headless/test/data as expected. We should fix this.
  HeadlessWebContents* web_contents =
      browser()
          ->CreateWebContentsBuilder()
          .SetInitialURL(GURL("http://not-an-actual-domain.tld/hello.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));
  EXPECT_EQ(static_cast<size_t>(1), browser()->GetAllWebContents().size());
  EXPECT_EQ(web_contents, browser()->GetAllWebContents()[0]);
  web_contents->Close();
  EXPECT_TRUE(browser()->GetAllWebContents().empty());
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, SetHostResolverRules) {
  EXPECT_TRUE(embedded_test_server()->Start());
  HeadlessBrowser::Options::Builder builder;
  builder.SetHostResolverRules(
      base::StringPrintf("MAP not-an-actual-domain.tld 127.0.0.1:%d",
                         embedded_test_server()->host_port_pair().port()));
  SetBrowserOptions(builder.Build());

  // Load a page which doesn't actually exist, but which is turned into a valid
  // address by our host resolver rules.
  HeadlessWebContents* web_contents =
      browser()
          ->CreateWebContentsBuilder()
          .SetInitialURL(GURL("http://not-an-actual-domain.tld/hello.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, HttpProtocolHandler) {
  const std::string kResponseBody = "<p>HTTP response body</p>";
  ProtocolHandlerMap protocol_handlers;
  protocol_handlers[url::kHttpScheme] =
      base::WrapUnique(new TestProtocolHandler(kResponseBody));

  HeadlessBrowser::Options::Builder builder;
  builder.SetProtocolHandlers(std::move(protocol_handlers));
  SetBrowserOptions(builder.Build());

  // Load a page which doesn't actually exist, but which is fetched by our
  // custom protocol handler.
  HeadlessWebContents* web_contents =
      browser()
          ->CreateWebContentsBuilder()
          .SetInitialURL(GURL("http://not-an-actual-domain.tld/hello.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));

  std::string inner_html;
  EXPECT_TRUE(EvaluateScript(web_contents, "document.body.innerHTML")
                  ->GetResult()
                  ->GetValue()
                  ->GetAsString(&inner_html));
  EXPECT_EQ(kResponseBody, inner_html);
}

IN_PROC_BROWSER_TEST_F(HeadlessBrowserTest, HttpsProtocolHandler) {
  const std::string kResponseBody = "<p>HTTPS response body</p>";
  ProtocolHandlerMap protocol_handlers;
  protocol_handlers[url::kHttpsScheme] =
      base::WrapUnique(new TestProtocolHandler(kResponseBody));

  HeadlessBrowser::Options::Builder builder;
  builder.SetProtocolHandlers(std::move(protocol_handlers));
  SetBrowserOptions(builder.Build());

  // Load a page which doesn't actually exist, but which is fetched by our
  // custom protocol handler.
  HeadlessWebContents* web_contents =
      browser()
          ->CreateWebContentsBuilder()
          .SetInitialURL(GURL("https://not-an-actual-domain.tld/hello.html"))
          .Build();
  EXPECT_TRUE(WaitForLoad(web_contents));

  std::string inner_html;
  EXPECT_TRUE(EvaluateScript(web_contents, "document.body.innerHTML")
                  ->GetResult()
                  ->GetValue()
                  ->GetAsString(&inner_html));
  EXPECT_EQ(kResponseBody, inner_html);
}

namespace {
const char kMainPageCookie[] = "mood=quizzical";
const char kIsolatedPageCookie[] = "mood=quixotic";
}  // namespace

// This test creates two tabs pointing to the same security origin in two
// different browser contexts and checks that they are isolated by creating two
// cookies with the same name in both tabs. The steps are:
//
// 1. Wait for tab #1 to become ready for DevTools.
// 2. Create tab #2 and wait for it to become ready for DevTools.
// 3. Navigate tab #1 to the test page and wait for it to finish loading.
// 4. Navigate tab #2 to the test page and wait for it to finish loading.
// 5. Set a cookie in tab #1.
// 6. Set the same cookie in tab #2 to a different value.
// 7. Read the cookie in tab #1 and check that it has the first value.
// 8. Read the cookie in tab #2 and check that it has the second value.
//
// If the tabs aren't properly isolated, step 7 will fail.
class HeadlessBrowserContextIsolationTest
    : public HeadlessAsyncDevTooledBrowserTest {
 public:
  HeadlessBrowserContextIsolationTest()
      : web_contents2_(nullptr),
        devtools_client2_(HeadlessDevToolsClient::Create()) {
    EXPECT_TRUE(embedded_test_server()->Start());
  }

  // HeadlessWebContentsObserver implementation:
  void DevToolsTargetReady() override {
    if (!web_contents2_) {
      browser_context_ = browser()->CreateBrowserContextBuilder().Build();
      web_contents2_ = browser()
                           ->CreateWebContentsBuilder()
                           .SetBrowserContext(browser_context_.get())
                           .Build();
      web_contents2_->AddObserver(this);
      return;
    }

    web_contents2_->GetDevToolsTarget()->AttachClient(devtools_client2_.get());
    HeadlessAsyncDevTooledBrowserTest::DevToolsTargetReady();
  }

  void RunDevTooledTest() override {
    load_observer_.reset(new LoadObserver(
        devtools_client_.get(),
        base::Bind(&HeadlessBrowserContextIsolationTest::OnFirstLoadComplete,
                   base::Unretained(this))));
    devtools_client_->GetPage()->Navigate(
        embedded_test_server()->GetURL("/hello.html").spec());
  }

  void OnFirstLoadComplete() {
    EXPECT_TRUE(load_observer_->navigation_succeeded());
    load_observer_.reset(new LoadObserver(
        devtools_client2_.get(),
        base::Bind(&HeadlessBrowserContextIsolationTest::OnSecondLoadComplete,
                   base::Unretained(this))));
    devtools_client2_->GetPage()->Navigate(
        embedded_test_server()->GetURL("/hello.html").spec());
  }

  void OnSecondLoadComplete() {
    EXPECT_TRUE(load_observer_->navigation_succeeded());
    load_observer_.reset();

    devtools_client_->GetRuntime()->Evaluate(
        base::StringPrintf("document.cookie = '%s'", kMainPageCookie),
        base::Bind(&HeadlessBrowserContextIsolationTest::OnFirstSetCookieResult,
                   base::Unretained(this)));
  }

  void OnFirstSetCookieResult(std::unique_ptr<runtime::EvaluateResult> result) {
    std::string cookie;
    EXPECT_TRUE(result->GetResult()->GetValue()->GetAsString(&cookie));
    EXPECT_EQ(kMainPageCookie, cookie);

    devtools_client2_->GetRuntime()->Evaluate(
        base::StringPrintf("document.cookie = '%s'", kIsolatedPageCookie),
        base::Bind(
            &HeadlessBrowserContextIsolationTest::OnSecondSetCookieResult,
            base::Unretained(this)));
  }

  void OnSecondSetCookieResult(
      std::unique_ptr<runtime::EvaluateResult> result) {
    std::string cookie;
    EXPECT_TRUE(result->GetResult()->GetValue()->GetAsString(&cookie));
    EXPECT_EQ(kIsolatedPageCookie, cookie);

    devtools_client_->GetRuntime()->Evaluate(
        "document.cookie",
        base::Bind(&HeadlessBrowserContextIsolationTest::OnFirstGetCookieResult,
                   base::Unretained(this)));
  }

  void OnFirstGetCookieResult(std::unique_ptr<runtime::EvaluateResult> result) {
    std::string cookie;
    EXPECT_TRUE(result->GetResult()->GetValue()->GetAsString(&cookie));
    EXPECT_EQ(kMainPageCookie, cookie);

    devtools_client2_->GetRuntime()->Evaluate(
        "document.cookie",
        base::Bind(
            &HeadlessBrowserContextIsolationTest::OnSecondGetCookieResult,
            base::Unretained(this)));
  }

  void OnSecondGetCookieResult(
      std::unique_ptr<runtime::EvaluateResult> result) {
    std::string cookie;
    EXPECT_TRUE(result->GetResult()->GetValue()->GetAsString(&cookie));
    EXPECT_EQ(kIsolatedPageCookie, cookie);
    FinishTest();
  }

  void FinishTest() {
    web_contents2_->RemoveObserver(this);
    web_contents2_->Close();
    browser_context_.reset();
    FinishAsynchronousTest();
  }

 private:
  std::unique_ptr<HeadlessBrowserContext> browser_context_;
  HeadlessWebContents* web_contents2_;
  std::unique_ptr<HeadlessDevToolsClient> devtools_client2_;
  std::unique_ptr<LoadObserver> load_observer_;
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(HeadlessBrowserContextIsolationTest);

}  // namespace headless
