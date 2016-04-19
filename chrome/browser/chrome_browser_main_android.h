// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_MAIN_ANDROID_H_
#define CHROME_BROWSER_CHROME_BROWSER_MAIN_ANDROID_H_

#include "base/macros.h"
#include "chrome/browser/chrome_browser_main.h"

namespace breakpad {
class CrashDumpManager;
}

class ChromeBrowserMainPartsAndroid : public ChromeBrowserMainParts {
 public:
  explicit ChromeBrowserMainPartsAndroid(
      const content::MainFunctionParams& parameters);
  ~ChromeBrowserMainPartsAndroid() override;

  // content::BrowserMainParts overrides.
  int PreCreateThreads() override;
  void PostProfileInit() override;
  void PreEarlyInitialization() override;
  void PreMainMessageLoopRun() override;

  // ChromeBrowserMainParts overrides.
  void PostBrowserStart() override;
  void ShowMissingLocaleMessageBox() override;

 private:
  std::unique_ptr<base::MessageLoop> main_message_loop_;
  std::unique_ptr<breakpad::CrashDumpManager> crash_dump_manager_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainPartsAndroid);
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_MAIN_ANDROID_H_
