// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/automation.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/test_launcher_utils.h"
#include "googleurl/src/gurl.h"

namespace webdriver {

void Automation::Init(bool* success) {
  *success = false;

  // Create a temp directory for the new profile.
  if (!profile_dir_.CreateUniqueTempDir()) {
    LOG(ERROR) << "Could not make a temp profile directory";
    return;
  }
  // TODO(kkania): See if these are still needed.
  test_launcher_utils::PrepareBrowserCommandLineForTests(&launch_arguments_);
  launch_arguments_.AppendSwitch(switches::kDomAutomationController);
  launch_arguments_.AppendSwitch(switches::kFullMemoryCrashReport);

  launch_arguments_.AppendSwitchPath(switches::kUserDataDir,
                                     profile_dir_.path());
  UITestBase::SetUp();
  browser_ = automation()->GetBrowserWindow(0);
  if (!browser_.get()) {
    Terminate();
    return;
  }
  tab_ = browser_->GetActiveTab();
  if (!tab_.get()) {
    Terminate();
    return;
  }
  *success = true;
}

void Automation::Terminate() {
  QuitBrowser();
}

void Automation::ExecuteScript(const std::string& frame_xpath,
                               const std::string& script,
                               std::string* result,
                               bool* success) {
  std::wstring wide_xpath = UTF8ToWide(frame_xpath);
  std::wstring wide_script = UTF8ToWide(script);
  std::wstring wide_result;
  *success = tab_->ExecuteAndExtractString(
      wide_xpath, wide_script, &wide_result);
  if (*success)
    *result = WideToUTF8(wide_result);
}

void Automation::NavigateToURL(const std::string& url,
                               bool* success) {
  *success = tab_->NavigateToURL(GURL(url));
}

void Automation::GoForward(bool* success) {
  *success = tab_->GoForward();
}

void Automation::GoBack(bool* success) {
  *success = tab_->GoBack();
}

void Automation::Reload(bool* success) {
  *success = tab_->Reload();
}

void Automation::GetURL(std::string* url,
                        bool* success) {
  GURL gurl;
  *success = tab_->GetCurrentURL(&gurl);
  if (*success)
    *url = gurl.possibly_invalid_spec();
}

void Automation::GetTabTitle(std::string* tab_title,
                             bool* success) {
  std::wstring wide_title;
  *success = tab_->GetTabTitle(&wide_title);
  if (*success)
    *tab_title = WideToUTF8(wide_title);
}

}  // namespace webdriver
