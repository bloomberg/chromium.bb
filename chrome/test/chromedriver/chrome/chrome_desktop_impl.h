// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_DESKTOP_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_DESKTOP_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/process.h"
#include "chrome/test/chromedriver/chrome/chrome_impl.h"

namespace base {
class TimeDelta;
}

class AutomationExtension;
class DevToolsHttpClient;
class Status;
class WebView;

class ChromeDesktopImpl : public ChromeImpl {
 public:
  ChromeDesktopImpl(
      scoped_ptr<DevToolsHttpClient> client,
      ScopedVector<DevToolsEventListener>& devtools_event_listeners,
      Log* log,
      base::ProcessHandle process,
      base::ScopedTempDir* user_data_dir,
      base::ScopedTempDir* extension_dir);
  virtual ~ChromeDesktopImpl();

  // Waits for a page with the given URL to appear and finish loading.
  // Returns an error if the timeout is exceeded.
  Status WaitForPageToLoad(const std::string& url,
                           const base::TimeDelta& timeout,
                           scoped_ptr<WebView>* web_view);

  // Overridden from Chrome:
  virtual Type GetType() OVERRIDE;
  virtual Status GetAutomationExtension(
      AutomationExtension** extension) OVERRIDE;
  virtual std::string GetOperatingSystemName() OVERRIDE;
  virtual Status Quit() OVERRIDE;

 private:
  base::ProcessHandle process_;
  bool quit_;
  base::ScopedTempDir user_data_dir_;
  base::ScopedTempDir extension_dir_;

  // Lazily initialized, may be null.
  scoped_ptr<AutomationExtension> automation_extension_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_DESKTOP_IMPL_H_
