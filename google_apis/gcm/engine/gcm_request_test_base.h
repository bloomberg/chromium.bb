// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_GCM_ENGINE_GCM_REQUEST_TEST_BASE_H_
#define GOOGLE_APIS_GCM_ENGINE_GCM_REQUEST_TEST_BASE_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/backoff_entry.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace gcm {

// The base class for testing various GCM requests that contains all the common
// logic to set up a task runner and complete a request with retries.
class GCMRequestTestBase : public testing::Test {
 public:
  GCMRequestTestBase();
  ~GCMRequestTestBase() override;

  const net::BackoffEntry::Policy& GetBackoffPolicy() const;
  net::TestURLFetcher* GetFetcher() const;

  // Set the response status and body that will be returned by the URL fetch.
  void SetResponse(net::HttpStatusCode status_code,
                   const std::string& response_body);

  // Completes the URL fetch.
  // It can be overridden by the test class to add additional logic.
  virtual void CompleteFetch();

  // Verifies that the Fetcher's upload_data exactly matches the given
  // properties. The map will be cleared as a side-effect. Wrap calls to this
  // with ASSERT_NO_FATAL_FAILURE.
  void VerifyFetcherUploadData(
      std::map<std::string, std::string>* expected_pairs);

  net::URLRequestContextGetter* url_request_context_getter() const {
    return url_request_context_getter_.get();
  }

  // The code is in transition away from URLRequestContextGetter +
  // URLFetcherFactory to SharedURLLoaderFactory. For now, both are needed.
  // Things that use url_loader_factory() will be matched with test APIs with
  // "ForURL" in their names below.
  network::SharedURLLoaderFactory* url_loader_factory() const {
    return shared_factory_.get();
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

  // This is a version for the TestURLLoaderFactory path; it also needs a URL.
  void SetResponseForURLAndComplete(const std::string& url,
                                    net::HttpStatusCode status_code,
                                    const std::string& response_body,
                                    int net_error_code = net::OK);

  // Note: may return null if URL isn't pending.
  const net::HttpRequestHeaders* GetExtraHeadersForURL(const std::string& url);

  // Returns false if URL isn't pending or extraction failed.
  bool GetUploadDataForURL(const std::string& url, std::string* data_out);

  // See docs for VerifyFetcherUploadData.
  void VerifyFetcherUploadDataForURL(
      const std::string& url,
      std::map<std::string, std::string>* expected_pairs);

 private:
  // Fast forward the timer used in the test to retry the request immediately.
  void FastForwardToTriggerNextRetry();

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  scoped_refptr<net::TestURLRequestContextGetter> url_request_context_getter_;
  net::TestURLFetcherFactory url_fetcher_factory_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_factory_;

  // Tracks the number of retries so far.
  int retry_count_;

  DISALLOW_COPY_AND_ASSIGN(GCMRequestTestBase);
};

}  // namespace gcm

#endif  // GOOGLE_APIS_GCM_ENGINE_GCM_REQUEST_TEST_BASE_H_
