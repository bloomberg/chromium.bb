// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_test.h"
#include "headless/public/devtools/domains/network.h"
#include "headless/public/devtools/domains/page.h"
#include "headless/public/headless_devtools_client.h"
#include "headless/public/util/expedited_dispatcher.h"
#include "headless/public/util/generic_url_request_job.h"
#include "headless/public/util/url_fetcher.h"
#include "headless/test/headless_browser_test.h"
#include "net/url_request/url_request_job_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::ContainerEq;

namespace headless {

namespace {
// Keep in sync with X_DevTools_Request_Id defined in HTTPNames.json5.
const char kDevtoolsRequestId[] = "X-DevTools-Request-Id";
}  // namespace

namespace {
class RequestIdCorrelationProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  explicit RequestIdCorrelationProtocolHandler(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner)
      : test_delegate_(new TestDelegate(this)),
        dispatcher_(new ExpeditedDispatcher(io_thread_task_runner)) {}

  ~RequestIdCorrelationProtocolHandler() override {}

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
    explicit MockURLFetcher(
        const RequestIdCorrelationProtocolHandler* protocol_handler)
        : protocol_handler_(protocol_handler) {}
    ~MockURLFetcher() override {}

    // URLFetcher implementation:
    void StartFetch(const GURL& url,
                    const std::string& method,
                    const net::HttpRequestHeaders& request_headers,
                    ResultListener* result_listener) override {
      const Response* response = protocol_handler_->GetResponse(url.spec());
      if (!response)
        result_listener->OnFetchStartError(net::ERR_FILE_NOT_FOUND);

      // The header used for correlation should not be sent to the fetcher.
      EXPECT_FALSE(request_headers.HasHeader(kDevtoolsRequestId));

      result_listener->OnFetchCompleteExtractHeaders(
          url, 200, response->data.c_str(), response->data.size());
    }

   private:
    const RequestIdCorrelationProtocolHandler* protocol_handler_;

    DISALLOW_COPY_AND_ASSIGN(MockURLFetcher);
  };

  class TestDelegate : public GenericURLRequestJob::Delegate {
   public:
    explicit TestDelegate(RequestIdCorrelationProtocolHandler* protocol_handler)
        : protocol_handler_(protocol_handler) {}
    ~TestDelegate() override {}

    // GenericURLRequestJob::Delegate implementation:
    bool BlockOrRewriteRequest(
        const GURL& url,
        const std::string& devtools_id,
        const std::string& method,
        const std::string& referrer,
        GenericURLRequestJob::RewriteCallback callback) override {
      protocol_handler_->url_to_devtools_id_[url.spec()] = devtools_id;
      return false;
    }

    const GenericURLRequestJob::HttpResponse* MaybeMatchResource(
        const GURL& url,
        const std::string& devtools_id,
        const std::string& method,
        const net::HttpRequestHeaders& request_headers) override {
      return nullptr;
    }

    void OnResourceLoadComplete(const GURL& final_url,
                                const std::string& devtools_id,
                                const std::string& mime_type,
                                int http_response_code) override {}

   private:
    RequestIdCorrelationProtocolHandler* protocol_handler_;

    DISALLOW_COPY_AND_ASSIGN(TestDelegate);
  };

  // net::URLRequestJobFactory::ProtocolHandler implementation::
  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    return new GenericURLRequestJob(
        request, network_delegate, dispatcher_.get(),
        base::MakeUnique<MockURLFetcher>(this), test_delegate_.get());
  }

  std::map<std::string, std::string> url_to_devtools_id_;

 private:
  std::unique_ptr<TestDelegate> test_delegate_;
  std::unique_ptr<ExpeditedDispatcher> dispatcher_;
  std::map<std::string, Response> response_map_;

  DISALLOW_COPY_AND_ASSIGN(RequestIdCorrelationProtocolHandler);
};

const char* kIndexHtml = R"(
<html>
<head>
<link rel="stylesheet" type="text/css" href="style1.css">
<link rel="stylesheet" type="text/css" href="style2.css">
</head>
<body>Hello.
</body>
</html>)";

const char* kStyle1 = R"(
.border {
  border: 1px solid #000;
})";

const char* kStyle2 = R"(
.border {
  border: 2px solid #fff;
})";

}  // namespace

class ProtocolHandlerRequestIdCorrelationTest
    : public HeadlessAsyncDevTooledBrowserTest,
      public network::Observer,
      public page::Observer {
 public:
  void RunDevTooledTest() override {
    EXPECT_TRUE(embedded_test_server()->Start());
    devtools_client_->GetPage()->AddObserver(this);
    devtools_client_->GetPage()->Enable();
    devtools_client_->GetNetwork()->AddObserver(this);
    devtools_client_->GetNetwork()->Enable();
    devtools_client_->GetPage()->Navigate("http://foo.com/index.html");
  }

  ProtocolHandlerMap GetProtocolHandlers() override {
    ProtocolHandlerMap protocol_handlers;
    std::unique_ptr<RequestIdCorrelationProtocolHandler> http_handler(
        new RequestIdCorrelationProtocolHandler(browser()->BrowserIOThread()));
    http_handler_ = http_handler.get();
    http_handler_->InsertResponse("http://foo.com/index.html",
                                  {kIndexHtml, "text/html"});
    http_handler_->InsertResponse("http://foo.com/style1.css",
                                  {kStyle1, "text/css"});
    http_handler_->InsertResponse("http://foo.com/style2.css",
                                  {kStyle2, "text/css"});
    protocol_handlers[url::kHttpScheme] = std::move(http_handler);
    return protocol_handlers;
  }

  // network::Observer implementation:
  void OnRequestWillBeSent(
      const network::RequestWillBeSentParams& params) override {
    url_to_devtools_id_[params.GetRequest()->GetUrl()] = params.GetRequestId();
    EXPECT_FALSE(params.GetRequest()->GetHeaders()->HasKey(kDevtoolsRequestId));
  }

  // page::Observer implementation:
  void OnLoadEventFired(const page::LoadEventFiredParams& params) override {
    // Make sure that our protocol handler saw the same url : devtools ids as
    // our OnRequestWillBeSent event listener did.
    EXPECT_THAT(url_to_devtools_id_,
                ContainerEq(http_handler_->url_to_devtools_id_));
    EXPECT_EQ(3u, url_to_devtools_id_.size());
    FinishAsynchronousTest();
  }

 private:
  std::map<std::string, std::string> url_to_devtools_id_;
  RequestIdCorrelationProtocolHandler* http_handler_;  // NOT OWNED
};

HEADLESS_ASYNC_DEVTOOLED_TEST_F(ProtocolHandlerRequestIdCorrelationTest);

}  // namespace headless
