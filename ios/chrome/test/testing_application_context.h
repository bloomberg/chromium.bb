// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_TESTING_APPLICATION_CONTEXT_H_
#define IOS_CHROME_TEST_TESTING_APPLICATION_CONTEXT_H_

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "ios/chrome/browser/application_context.h"

class TestingApplicationContext : public ApplicationContext {
 public:
  TestingApplicationContext();
  ~TestingApplicationContext() override;

  // Convenience method to get the current application context as a
  // TestingApplicationContext.
  static TestingApplicationContext* GetGlobal();

  // Sets the local state.
  void SetLocalState(PrefService* local_state);

  // Sets the ChromeBrowserStateManager.
  void SetChromeBrowserStateManager(ios::ChromeBrowserStateManager* manager);

  // ApplicationContext implementation.
  PrefService* GetLocalState() override;
  net::URLRequestContextGetter* GetSystemURLRequestContext() override;
  const std::string& GetApplicationLocale() override;
  ios::ChromeBrowserStateManager* GetChromeBrowserStateManager() override;
  metrics::MetricsService* GetMetricsService() override;
  policy::BrowserPolicyConnector* GetBrowserPolicyConnector() override;
  rappor::RapporService* GetRapporService() override;
  net_log::ChromeNetLog* GetNetLog() override;

 private:
  base::ThreadChecker thread_checker_;
  std::string application_locale_;
  PrefService* local_state_;
  ios::ChromeBrowserStateManager* chrome_browser_state_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestingApplicationContext);
};

#endif  // IOS_CHROME_TEST_TESTING_APPLICATION_CONTEXT_H_
