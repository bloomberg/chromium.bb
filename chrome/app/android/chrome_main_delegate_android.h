// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_ANDROID_CHROME_MAIN_DELEGATE_ANDROID_H_
#define CHROME_APP_ANDROID_CHROME_MAIN_DELEGATE_ANDROID_H_

#include <memory>

#include "base/macros.h"
#include "chrome/app/chrome_main_delegate.h"
#include "content/public/browser/browser_main_runner.h"

namespace safe_browsing {
class SafeBrowsingApiHandler;
}

// Android override of ChromeMainDelegate
class ChromeMainDelegateAndroid : public ChromeMainDelegate {
 public:
  static ChromeMainDelegateAndroid* Create();

 protected:
  ChromeMainDelegateAndroid();
  ~ChromeMainDelegateAndroid() override;

  bool BasicStartupComplete(int* exit_code) override;
  void SandboxInitialized(const std::string& process_type) override;
  int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) override;
  void ProcessExiting(const std::string& process_type) override;

#if defined(SAFE_BROWSING_DB_REMOTE)
  virtual safe_browsing::SafeBrowsingApiHandler* CreateSafeBrowsingApiHandler();
#endif

 private:
  std::unique_ptr<content::BrowserMainRunner> browser_runner_;

#if defined(SAFE_BROWSING_DB_REMOTE)
  std::unique_ptr<safe_browsing::SafeBrowsingApiHandler>
      safe_browsing_api_handler_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeMainDelegateAndroid);
};

#endif  // CHROME_APP_ANDROID_CHROME_MAIN_DELEGATE_ANDROID_H_
