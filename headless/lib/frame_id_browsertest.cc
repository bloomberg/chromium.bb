// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/public/test/browser_test.h"
#include "headless/lib/browser/headless_browser_context_impl.h"
#include "headless/lib/browser/headless_web_contents_impl.h"
#include "headless/public/devtools/domains/network.h"
#include "headless/public/devtools/domains/page.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/util/expedited_dispatcher.h"
#include "headless/public/util/generic_url_request_job.h"
#include "headless/public/util/url_fetcher.h"
#include "headless/test/headless_browser_test.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_job_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::ContainerEq;

namespace headless {

namespace {
class TestProtocolHandler : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  explicit TestProtocolHandler(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner)
      : test_delegate_(new TestDelegate(this)),
        dispatcher_(new ExpeditedDispatcher(io_thread_task_runner)),
        headless_browser_context_(nullptr) {}

  ~TestProtocolHandler() override {}

  void SetHeadlessBrowserContext(
      HeadlessBrowserContext* headless_browser_context) {
    headless_browser_context_ = headless_browser_context;
  }

  struct Response {
    Response() {}
    Response(const std::string& body, const std::string& mime_type)
        : data("HTTP/1.1 200 OK\r\nContent-Type: " + mime_type + "\r\n\r\n" +
               body) {}

    std::string data;
  };

  void InsertResponse(const std::string& url, const Response& response) {
    response_map_[url] = response;
  }

  const Response* GetResponse(const std::string& url) const {
    std::map<std::string, Response>::const_iterator find_it =
        response_map_.find(url);
    if (find_it == response_map_.end())
      return nullptr;
    return &find_it->second;
  }

  class MockURLFetcher : public URLFetcher {
   public:
    explicit MockURLFetcher(const TestProtocolHandler* protocol_handler)
        : protocol_handler_(protocol_handler) {}
    ~MockURLFetcher() override {}

    // URLFetcher implementation:
    void StartFetch(const GURL& url,
                    const std::string& method,
                    const std::string& post_data,
                    const net::HttpRequestHeaders& request_headers,
                    ResultListener* result_listener) override {
      EXPECT_EQ("GET", method);

      const Response* response = protocol_handler_->GetResponse(url.spec());
      if (!response)
        result_listener->OnFetchStartError(net::ERR_FILE_NOT_FOUND);

      result_listener->OnFetchCompleteExtractHeaders(
          url, response->data.c_str(), response->data.size());
    }

   private:
    const TestProtocolHandler* protocol_handler_;

    DISALLOW_COPY_AND_ASSIGN(MockURLFetcher);
  };

  class TestDelegate : public GenericURLRequestJob::Delegate {
   public:
    explicit TestDelegate(TestProtocolHandler* protocol_handler)
        : protocol_handler_(protocol_handler) {}

    ~TestDelegate() override {}

    // GenericURLRequestJob::Delegate implementation:
    void OnPendingRequest(PendingRequest* pending_request) override {
      const Request* request = pending_request->GetRequest();
      std::string url = request->GetURLRequest()->url().spec();
      int frame_tree_node_id = request->GetFrameTreeNodeId();
      DCHECK_NE(frame_tree_node_id, -1);
      protocol_handler_->url_to_frame_tree_node_id_[url] = frame_tree_node_id;
      pending_request->AllowRequest();
    }

    void OnResourceLoadFailed(const Request* request,
                              net::Error error) override {}

    void OnResourceLoadComplete(
        const Request* request,
        const GURL& final_url,
        scoped_refptr<net::HttpResponseHeaders> response_headers,
        const char* body,
        size_t body_size) override {}

   private:
    TestProtocolHandler* protocol_handler_;  // NOT OWNED
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner_;

    DISALLOW_COPY_AND_ASSIGN(TestDelegate);
  };

  // net::URLRequestJobFactory::ProtocolHandler implementation::
  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new GenericURLRequestJob(
        request, network_delegate, dispatcher_.get(),
        base::MakeUnique<MockURLFetcher>(this), test_delegate_.get(),
        headless_browser_context_);
  }

  std::map<std::string, int> url_to_frame_tree_node_id_;

 private:
  std::unique_ptr<TestDelegate> test_delegate_;
  std::unique_ptr<ExpeditedDispatcher> dispatcher_;
  std::map<std::string, Response> response_map_;
  HeadlessBrowserContext* headless_browser_context_;

  DISALLOW_COPY_AND_ASSIGN(TestProtocolHandler);
};

const char* kIndexHtml = R"(
<html>
<head><link rel="stylesheet" type="text/css" href="style1.css"></head>
<body>
<div class="red">Main frame</div>
<iframe src="iframe1.html"></iframe>
<iframe src="iframe2.html"></iframe>
</body>
</html>)";

const char* kIFrame1 = R"(
<html>
<head><link rel="stylesheet" type="text/css" href="style2.css"></head>
<body>
<div class="green">IFrame 1</div>
</body>
</html>)";

const char* kIFrame2 = R"(
<html>
<head><link rel="stylesheet" type="text/css" href="style3.css"></head>
<body>
<div class="blue">IFrame 1</div>
</body>
</html>)";

const char* kStyle1Css = R"(
.red {
  color: #f00
} )";

const char* kStyle2Css = R"(
.green {
  color: #0f0
} )";

const char* kStyle3Css = R"(
.blue {
  color: #00f
} )";

}  // namespace

class FrameIdTest : public HeadlessAsyncDevTooledBrowserTest,
                    public network::Observer,
                    public page::Observer {
 public:
  void RunDevTooledTest() override {
    http_handler_->SetHeadlessBrowserContext(browser_context_);

    EXPECT_TRUE(embedded_test_server()->Start());
    devtools_client_->GetNetwork()->AddObserver(this);
    devtools_client_->GetNetwork()->Enable();
    devtools_client_->GetPage()->AddObserver(this);

    base::RunLoop run_loop;
    devtools_client_->GetPage()->Enable(run_loop.QuitClosure());
    base::MessageLoop::ScopedNestableTaskAllower nest_loop(
        base::MessageLoop::current());
    run_loop.Run();

    devtools_client_->GetPage()->Navigate("http://foo.com/index.html");
  }

  ProtocolHandlerMap GetProtocolHandlers() override {
    ProtocolHandlerMap protocol_handlers;
    std::unique_ptr<TestProtocolHandler> http_handler(
        new TestProtocolHandler(browser()->BrowserIOThread()));
    http_handler_ = http_handler.get();
    http_handler_->InsertResponse("http://foo.com/index.html",
                                  {kIndexHtml, "text/html"});
    http_handler_->InsertResponse("http://foo.com/iframe1.html",
                                  {kIFrame1, "text/html"});
    http_handler_->InsertResponse("http://foo.com/iframe2.html",
                                  {kIFrame2, "text/html"});
    http_handler_->InsertResponse("http://foo.com/style1.css",
                                  {kStyle1Css, "text/css"});
    http_handler_->InsertResponse("http://foo.com/style2.css",
                                  {kStyle2Css, "text/css"});
    http_handler_->InsertResponse("http://foo.com/style3.css",
                                  {kStyle3Css, "text/css"});
    protocol_handlers[url::kHttpScheme] = std::move(http_handler);
    return protocol_handlers;
  }

  // network::Observer implementation:
  void OnRequestWillBeSent(
      const network::RequestWillBeSentParams& params) override {
    url_to_frame_id_[params.GetRequest()->GetUrl()] = params.GetFrameId();
  }

  // page::Observer implementation:
  void OnLoadEventFired(const page::LoadEventFiredParams& params) override {
    std::map<std::string, std::string> protocol_handler_url_to_frame_id_;
    for (const auto& pair : http_handler_->url_to_frame_tree_node_id_) {
      // TODO(alexclarke): This will probably break with OOPIF, fix this.
      // See https://bugs.chromium.org/p/chromium/issues/detail?id=715924
      protocol_handler_url_to_frame_id_[pair.first] =
          web_contents_->GetUntrustedDevToolsFrameIdForFrameTreeNodeId(
              web_contents_->GetMainFrameRenderProcessId(), pair.second);
    }

    EXPECT_THAT(url_to_frame_id_, protocol_handler_url_to_frame_id_);
    EXPECT_EQ(6u, url_to_frame_id_.size());
    FinishAsynchronousTest();
  }

 private:
  std::map<std::string, std::string> url_to_frame_id_;
  TestProtocolHandler* http_handler_;  // NOT OWNED
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(FrameIdTest);

}  // namespace headless
