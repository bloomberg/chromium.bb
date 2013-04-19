// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_DESKTOP_IMPL_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_DESKTOP_IMPL_H_

#include <list>
#include <string>

#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/process.h"
#include "chrome/test/chromedriver/chrome/chrome_impl.h"

class AutomationExtension;
class DevToolsHttpClient;
class Status;

class ChromeDesktopImpl : public ChromeImpl {
 public:
  ChromeDesktopImpl(
      scoped_ptr<DevToolsHttpClient> client,
      const std::string& version,
      int build_no,
      const std::list<DevToolsEventLogger*>& devtools_event_loggers,
      base::ProcessHandle process,
      base::ScopedTempDir* user_data_dir,
      base::ScopedTempDir* extension_dir);
  virtual ~ChromeDesktopImpl();

  // Overridden from Chrome:
  virtual Status GetAutomationExtension(
      AutomationExtension** extension) OVERRIDE;
  virtual std::string GetOperatingSystemName() OVERRIDE;
  virtual Status Quit() OVERRIDE;

 private:
  base::ProcessHandle process_;
  base::ScopedTempDir user_data_dir_;
  base::ScopedTempDir extension_dir_;

  // Lazily initialized, may be null.
  scoped_ptr<AutomationExtension> automation_extension_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_CHROME_DESKTOP_IMPL_H_
