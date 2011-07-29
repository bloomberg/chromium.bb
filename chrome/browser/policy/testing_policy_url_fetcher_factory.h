// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_TESTING_POLICY_URL_FETCHER_FACTORY_H_
#define CHROME_BROWSER_POLICY_TESTING_POLICY_URL_FETCHER_FACTORY_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/policy/logging_work_scheduler.h"
#include "content/common/url_fetcher.h"
#include "content/test/test_url_fetcher_factory.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace policy {

class TestingPolicyURLFetcher;

struct TestURLResponse {
  std::string response_data;
  int response_code;
};

// Creates mock URLFetchers whose behavior can be controlled in tests. To do so
// set mock expectations on the method |Intercept|.
class TestingPolicyURLFetcherFactory : public URLFetcher::Factory,
                                       public ScopedURLFetcherFactory {
 public:
  explicit TestingPolicyURLFetcherFactory(EventLogger* logger);
  virtual ~TestingPolicyURLFetcherFactory();

  virtual URLFetcher* CreateURLFetcher(
      int id,
      const GURL& url,
      URLFetcher::RequestType request_type,
      URLFetcher::Delegate* delegate);

  LoggingWorkScheduler* scheduler();

  // Called back by TestingPolicyURLFetcher objects. Uses |Intercept| to get
  // the response and notifies |logger_| of a network request event.
  void GetResponse(const std::string& auth_header,
                   const std::string& request_type,
                   TestURLResponse* response);

  // Place EXPECT_CALLs on this method to control the responses of the
  // produced URLFetchers. The response data should be copied into |response|.
  MOCK_METHOD3(Intercept, void(const std::string& auth_header,
                               const std::string& request_type,
                               TestURLResponse* response));

 private:
  EventLogger* logger_;
  scoped_ptr<LoggingWorkScheduler> scheduler_;

  base::WeakPtrFactory<TestingPolicyURLFetcherFactory> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestingPolicyURLFetcherFactory);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_TESTING_POLICY_URL_FETCHER_FACTORY_H_
