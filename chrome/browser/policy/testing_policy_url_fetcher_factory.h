// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_TESTING_POLICY_URL_FETCHER_FACTORY_H_
#define CHROME_BROWSER_POLICY_TESTING_POLICY_URL_FETCHER_FACTORY_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

class GURL;

namespace enterprise_management {
class DeviceManagementRequest;
}

namespace policy {

class EventLogger;
class LoggingWorkScheduler;
class TestingPolicyURLFetcher;

struct TestURLResponse {
  std::string response_data;
  int response_code;
};

// Creates mock URLFetchers whose behavior can be controlled in tests. To do so
// set mock expectations on the method |Intercept|.
class TestingPolicyURLFetcherFactory : public net::URLFetcherFactory,
                                       public net::ScopedURLFetcherFactory {
 public:
  explicit TestingPolicyURLFetcherFactory(EventLogger* logger);
  virtual ~TestingPolicyURLFetcherFactory();

  virtual net::URLFetcher* CreateURLFetcher(
      int id,
      const GURL& url,
      net::URLFetcher::RequestType request_type,
      net::URLFetcherDelegate* delegate) OVERRIDE;

  LoggingWorkScheduler* scheduler();

  // Called back by TestingPolicyURLFetcher objects. Uses |Intercept| to get
  // the response and notifies |logger_| of a network request event.
  void GetResponse(
      const std::string& auth_header,
      const std::string& request_type,
      const enterprise_management::DeviceManagementRequest& request,
      TestURLResponse* response);

  // Place EXPECT_CALLs on this method to control the responses of the
  // produced URLFetchers. The response data should be copied into |response|.
  MOCK_METHOD4(
      Intercept,
      void(const std::string& auth_header,
           const std::string& request_type,
           const enterprise_management::DeviceManagementRequest& request,
           TestURLResponse* response));

 private:
  EventLogger* logger_;
  scoped_ptr<LoggingWorkScheduler> scheduler_;

  base::WeakPtrFactory<TestingPolicyURLFetcherFactory> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestingPolicyURLFetcherFactory);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_TESTING_POLICY_URL_FETCHER_FACTORY_H_
