// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/automation.h"

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/test_launcher_utils.h"
#include "googleurl/src/gurl.h"

namespace webdriver {

Automation::Automation() {}

Automation::~Automation() {}

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

void Automation::SendWebKeyEvent(const WebKeyEvent& key_event,
                                 bool* success) {
  scoped_ptr<DictionaryValue> dict(new DictionaryValue);
  dict->SetString("command", "SendKeyEventToActiveTab");
  dict->SetInteger("type", key_event.type);
  dict->SetInteger("nativeKeyCode", key_event.key_code);
  dict->SetInteger("windowsKeyCode", key_event.key_code);
  dict->SetString("unmodifiedText", key_event.unmodified_text);
  dict->SetString("text", key_event.modified_text);
  dict->SetInteger("modifiers", key_event.modifiers);
  dict->SetBoolean("isSystemKey", false);
  std::string request;
  base::JSONWriter::Write(dict.get(), false, &request);
  std::string reply;
  *success = browser_->SendJSONRequest(request, &reply);
  if (!*success) {
    LOG(ERROR) << "Could not send web key event. Reply: " << reply;
  }
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

void Automation::GetGURL(GURL* gurl,
                         bool* success) {
  *success = tab_->GetCurrentURL(gurl);
}

void Automation::GetTabTitle(std::string* tab_title,
                             bool* success) {
  std::wstring wide_title;
  *success = tab_->GetTabTitle(&wide_title);
  if (*success)
    *tab_title = WideToUTF8(wide_title);
}

void Automation::GetCookies(const GURL& gurl,
                            std::string* cookies,
                            bool* success) {
  *success = tab_->GetCookies(gurl, cookies);
}

void Automation::GetCookieByName(const GURL& gurl,
                                 const std::string& cookie_name,
                                 std::string* cookie,
                                 bool* success) {
  *success = tab_->GetCookieByName(gurl, cookie_name, cookie);
}

void Automation::DeleteCookie(const GURL& gurl,
                              const std::string& cookie_name,
                              bool* success) {
  *success = tab_->DeleteCookie(gurl, cookie_name);
}

void Automation::SetCookie(const GURL& gurl,
                           const std::string& cookie,
                           bool* success) {
  *success = tab_->SetCookie(gurl, cookie);
}

}  // namespace webdriver
