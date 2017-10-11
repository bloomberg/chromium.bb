// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_UTIL_TESTING_TEST_IN_MEMORY_PROTOCOL_HANDLER_H_
#define HEADLESS_PUBLIC_UTIL_TESTING_TEST_IN_MEMORY_PROTOCOL_HANDLER_H_

#include <map>
#include <vector>

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "net/url_request/url_request_job_factory.h"

namespace headless {
class ExpeditedDispatcher;
class HeadlessBrowserContext;

class TestInMemoryProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  TestInMemoryProtocolHandler(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
      bool simulate_slow_fetch);

  ~TestInMemoryProtocolHandler() override;

  void SetHeadlessBrowserContext(
      HeadlessBrowserContext* headless_browser_context);

  struct Response {
    Response() {}
    Response(const std::string& body, const std::string& mime_type)
        : data("HTTP/1.1 200 OK\r\nContent-Type: " + mime_type + "\r\n\r\n" +
               body) {}

    std::string data;
  };

  void InsertResponse(const std::string& url, const Response& response);

  // net::URLRequestJobFactory::ProtocolHandler implementation::
  net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

  const std::map<std::string, std::string>& url_to_devtools_frame_id() const {
    return url_to_devtools_frame_id_;
  }

  const std::vector<std::string>& urls_requested() const {
    return urls_requested_;
  }

 private:
  const Response* GetResponse(const std::string& url) const;

  void RegisterUrl(const std::string& url, std::string& devtools_frame_id) {
    urls_requested_.push_back(url);
    url_to_devtools_frame_id_[url] = devtools_frame_id;
  }

  bool simulate_slow_fetch() const { return simulate_slow_fetch_; }

  class TestDelegate;
  class MockURLFetcher;
  friend class TestDelegate;
  friend class MockURLFetcher;

  std::unique_ptr<TestDelegate> test_delegate_;
  std::unique_ptr<ExpeditedDispatcher> dispatcher_;
  std::map<std::string, Response> response_map_;
  HeadlessBrowserContext* headless_browser_context_;
  std::map<std::string, std::string> url_to_devtools_frame_id_;
  std::vector<std::string> urls_requested_;
  bool simulate_slow_fetch_;
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(TestInMemoryProtocolHandler);
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_UTIL_TESTING_TEST_IN_MEMORY_PROTOCOL_HANDLER_H_
