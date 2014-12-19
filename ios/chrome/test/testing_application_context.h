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

  // ApplicationContext implementation.
  PrefService* GetLocalState() override;
  net::URLRequestContextGetter* GetSystemURLRequestContext() override;
  const std::string& GetApplicationLocale() override;

 private:
  base::ThreadChecker thread_checker_;
  std::string application_locale_;
  PrefService* local_state_;

  DISALLOW_COPY_AND_ASSIGN(TestingApplicationContext);
};

#endif  // IOS_CHROME_TEST_TESTING_APPLICATION_CONTEXT_H_
