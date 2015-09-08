// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CHROME_SYNC_SHELL_MAIN_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_CHROME_SYNC_SHELL_MAIN_DELEGATE_H_

#include "chrome/app/android/chrome_main_delegate_android.h"

class ChromeSyncShellMainDelegate : public ChromeMainDelegateAndroid {
 public:
  ChromeSyncShellMainDelegate();
  ~ChromeSyncShellMainDelegate() override;

 protected:
  int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeSyncShellMainDelegate);
};

#endif  // CHROME_BROWSER_ANDROID_CHROME_SYNC_SHELL_MAIN_DELEGATE_H_
