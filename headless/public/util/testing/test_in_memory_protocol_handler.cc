// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/testing/test_in_memory_protocol_handler.h"

#include "base/memory/ptr_util.h"
#include "headless/public/util/expedited_dispatcher.h"
#include "headless/public/util/generic_url_request_job.h"
#include "headless/public/util/url_fetcher.h"

namespace headless {

class TestInMemoryProtocolHandler::MockURLFetcher : public URLFetcher {
 public:
  explicit MockURLFetcher(TestInMemoryProtocolHandler* protocol_handler)
      : protocol_handler_(protocol_handler) {}
  ~MockURLFetcher() override {}

  // URLFetcher implementation:
  void StartFetch(const Request* request,
                  ResultListener* result_listener) override {
    GURL url = request->GetURLRequest()->url();
    DCHECK_EQ("GET", request->GetURLRequest()->method());

    std::string devtools_frame_id = request->GetDevToolsFrameId();
    DCHECK_NE(devtools_frame_id, "") << " For url " << url;
    protocol_handler_->RegisterUrl(url.spec(), devtools_frame_id);

    if (protocol_handler_->request_deferrer()) {
      protocol_handler_->request_deferrer()->OnRequest(
          url, base::Bind(&MockURLFetcher::FinishFetch, base::Unretained(this),
                          result_listener, url));
    } else {
      FinishFetch(result_listener, url);
    }
  }

  void FinishFetch(ResultListener* result_listener, GURL url) {
    const TestInMemoryProtocolHandler::Response* response =
        protocol_handler_->GetResponse(url.spec());
    if (response) {
      net::LoadTimingInfo load_timing_info;
      result_listener->OnFetchCompleteExtractHeaders(
          url, response->data.c_str(), response->data.size(), load_timing_info);
    } else {
      result_listener->OnFetchStartError(net::ERR_FILE_NOT_FOUND);
    }
  }

 private:
  TestInMemoryProtocolHandler* protocol_handler_;  // NOT OWNED.

  DISALLOW_COPY_AND_ASSIGN(MockURLFetcher);
};

class TestInMemoryProtocolHandler::TestDelegate
    : public GenericURLRequestJob::Delegate {
 public:
  TestDelegate() {}
  ~TestDelegate() override {}

  // GenericURLRequestJob::Delegate implementation:
  void OnResourceLoadFailed(const Request* request, net::Error error) override {
  }

  void OnResourceLoadComplete(
      const Request* request,
      const GURL& final_url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      const char* body,
      size_t body_size) override {}

 private:
  scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

TestInMemoryProtocolHandler::TestInMemoryProtocolHandler(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
    RequestDeferrer* request_deferrer)
    : test_delegate_(new TestDelegate()),
      dispatcher_(new ExpeditedDispatcher(io_thread_task_runner)),
      headless_browser_context_(nullptr),
      request_deferrer_(request_deferrer),
      io_thread_task_runner_(io_thread_task_runner) {}

TestInMemoryProtocolHandler::~TestInMemoryProtocolHandler() {}

void TestInMemoryProtocolHandler::SetHeadlessBrowserContext(
    HeadlessBrowserContext* headless_browser_context) {
  headless_browser_context_ = headless_browser_context;
}

void TestInMemoryProtocolHandler::InsertResponse(const std::string& url,
                                                 const Response& response) {
  response_map_[url] = response;
}

const TestInMemoryProtocolHandler::Response*
TestInMemoryProtocolHandler::GetResponse(const std::string& url) const {
  std::map<std::string, Response>::const_iterator find_it =
      response_map_.find(url);
  if (find_it == response_map_.end())
    return nullptr;
  return &find_it->second;
}

net::URLRequestJob* TestInMemoryProtocolHandler::MaybeCreateJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  return new GenericURLRequestJob(
      request, network_delegate, dispatcher_.get(),
      base::MakeUnique<MockURLFetcher>(
          const_cast<TestInMemoryProtocolHandler*>(this)),
      test_delegate_.get(), headless_browser_context_);
}

}  // namespace headless
