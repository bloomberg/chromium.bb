// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CHROME_MAIN_DELEGATE_STAGING_ANDROID_H_
#define CHROME_BROWSER_ANDROID_CHROME_MAIN_DELEGATE_STAGING_ANDROID_H_

#include "chrome/app/android/chrome_main_delegate_android.h"

class SafeBrowsingApiHandler;
class DataReductionProxyResourceThrottleFactory;

class ChromeMainDelegateStagingAndroid : public ChromeMainDelegateAndroid {
 public:
  ChromeMainDelegateStagingAndroid();
  ~ChromeMainDelegateStagingAndroid() override;

 protected:
  int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) override;
#if defined(SAFE_BROWSING_DB_REMOTE)
  virtual SafeBrowsingApiHandler* CreateSafeBrowsingApiHandler();
#endif

 private:
  bool BasicStartupComplete(int* exit_code) override;
  void ProcessExiting(const std::string& process_type) override;

#if defined(SAFE_BROWSING_DB_REMOTE)
  scoped_ptr<SafeBrowsingApiHandler> safe_browsing_api_handler_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeMainDelegateStagingAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_CHROME_MAIN_DELEGATE_STAGING_ANDROID_H_
