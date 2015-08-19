// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ANDROID_SYNC_SHELL_CHROME_MAIN_DELEGATE_CHROME_SYNC_SHELL_ANDROID_H_
#define CHROME_ANDROID_SYNC_SHELL_CHROME_MAIN_DELEGATE_CHROME_SYNC_SHELL_ANDROID_H_

#include "chrome/app/android/chrome_main_delegate_android.h"

class ChromeMainDelegateChromeSyncShellAndroid
    : public ChromeMainDelegateAndroid {
 public:
  ChromeMainDelegateChromeSyncShellAndroid();
  ~ChromeMainDelegateChromeSyncShellAndroid() override;

  bool BasicStartupComplete(int* exit_code) override;
  int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeMainDelegateChromeSyncShellAndroid);
};

#endif  // CHROME_ANDROID_SYNC_SHELL_CHROME_MAIN_DELEGATE_CHROME_SYNC_SHELL_ANDROID_H_
