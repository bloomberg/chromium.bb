// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "content/public/test/browser_test.h"
#include "headless/public/domains/types.h"
#include "headless/public/headless_browser.h"
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
      browser()->CreateWebContents(GURL("about:blank"), gfx::Size(800, 600));
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
      browser()->CreateWebContents(bad_url, gfx::Size(800, 600));
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
  HeadlessWebContents* web_contents = browser()->CreateWebContents(
      GURL("http://not-an-actual-domain.tld/hello.html"), gfx::Size(800, 600));
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
  HeadlessWebContents* web_contents = browser()->CreateWebContents(
      GURL("http://not-an-actual-domain.tld/hello.html"), gfx::Size(800, 600));
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
  HeadlessWebContents* web_contents = browser()->CreateWebContents(
      GURL("http://not-an-actual-domain.tld/hello.html"), gfx::Size(800, 600));
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
  HeadlessWebContents* web_contents = browser()->CreateWebContents(
      GURL("https://not-an-actual-domain.tld/hello.html"), gfx::Size(800, 600));
  EXPECT_TRUE(WaitForLoad(web_contents));

  std::string inner_html;
  EXPECT_TRUE(EvaluateScript(web_contents, "document.body.innerHTML")
                  ->GetResult()
                  ->GetValue()
                  ->GetAsString(&inner_html));
  EXPECT_EQ(kResponseBody, inner_html);
}

}  // namespace headless
