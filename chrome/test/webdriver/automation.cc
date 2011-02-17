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
#include "chrome/test/automation/automation_proxy.h"
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
  *success = true;
}

void Automation::Terminate() {
  QuitBrowser();
}

void Automation::ExecuteScript(int tab_id,
                               const std::string& frame_xpath,
                               const std::string& script,
                               std::string* result,
                               bool* success) {
  TabProxy* tab = GetTabById(tab_id);
  if (!tab) {
    *success = false;
    return;
  }
  std::wstring wide_xpath = UTF8ToWide(frame_xpath);
  std::wstring wide_script = UTF8ToWide(script);
  std::wstring wide_result;
  *success = tab->ExecuteAndExtractString(
      wide_xpath, wide_script, &wide_result);
  if (*success)
    *result = WideToUTF8(wide_result);
}

void Automation::SendWebKeyEvent(int tab_id,
                                 const WebKeyEvent& key_event,
                                 bool* success) {
  TabProxy* tab = GetTabById(tab_id);
  if (!tab) {
    LOG(ERROR) << "No such tab";
    *success = false;
    return;
  }
  int tab_index = 0;
  if (!tab->GetTabIndex(&tab_index)) {
    LOG(ERROR) << "Could not get tab index";
    *success = false;
    return;
  }
  scoped_refptr<BrowserProxy> browser = tab->GetParentBrowser();
  if (!browser.get()) {
    LOG(ERROR) << "Could not get parent browser of tab";
    *success = false;
    return;
  }
  if (!browser->ActivateTab(tab_index)) {
    LOG(ERROR) << "Could not activate tab to send keys";
    *success = false;
    return;
  }
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
  *success = browser->SendJSONRequest(request, &reply);
  if (!*success) {
    LOG(ERROR) << "Could not send web key event. Reply: " << reply;
  }
}

void Automation::NavigateToURL(int tab_id,
                               const std::string& url,
                               bool* success) {
  TabProxy* tab = GetTabById(tab_id);
  if (!tab) {
    *success = false;
    return;
  }
  *success = tab->NavigateToURL(GURL(url));
}

void Automation::GoForward(int tab_id, bool* success) {
  TabProxy* tab = GetTabById(tab_id);
  if (!tab) {
    *success = false;
    return;
  }
  *success = tab->GoForward();
}

void Automation::GoBack(int tab_id, bool* success) {
  TabProxy* tab = GetTabById(tab_id);
  if (!tab) {
    *success = false;
    return;
  }
  *success = tab->GoBack();
}

void Automation::Reload(int tab_id, bool* success) {
  TabProxy* tab = GetTabById(tab_id);
  if (!tab) {
    *success = false;
    return;
  }
  *success = tab->Reload();
}

void Automation::GetURL(int tab_id,
                        std::string* url,
                        bool* success) {
  TabProxy* tab = GetTabById(tab_id);
  if (!tab) {
    *success = false;
    return;
  }
  GURL gurl;
  *success = tab->GetCurrentURL(&gurl);
  if (*success)
    *url = gurl.possibly_invalid_spec();
}

void Automation::GetGURL(int tab_id,
                         GURL* gurl,
                         bool* success) {
  TabProxy* tab = GetTabById(tab_id);
  if (!tab) {
    *success = false;
    return;
  }
  *success = tab->GetCurrentURL(gurl);
}

void Automation::GetTabTitle(int tab_id,
                             std::string* tab_title,
                             bool* success) {
  TabProxy* tab = GetTabById(tab_id);
  if (!tab) {
    *success = false;
    return;
  }
  std::wstring wide_title;
  *success = tab->GetTabTitle(&wide_title);
  if (*success)
    *tab_title = WideToUTF8(wide_title);
}

void Automation::GetCookies(int tab_id,
                            const GURL& gurl,
                            std::string* cookies,
                            bool* success) {
  TabProxy* tab = GetTabById(tab_id);
  if (!tab) {
    *success = false;
    return;
  }
  *success = tab->GetCookies(gurl, cookies);
}

void Automation::GetCookieByName(int tab_id,
                                 const GURL& gurl,
                                 const std::string& cookie_name,
                                 std::string* cookie,
                                 bool* success) {
  TabProxy* tab = GetTabById(tab_id);
  if (!tab) {
    *success = false;
    return;
  }
  *success = tab->GetCookieByName(gurl, cookie_name, cookie);
}

void Automation::DeleteCookie(int tab_id,
                              const GURL& gurl,
                              const std::string& cookie_name,
                              bool* success) {
  TabProxy* tab = GetTabById(tab_id);
  if (!tab) {
    *success = false;
    return;
  }
  *success = tab->DeleteCookie(gurl, cookie_name);
}

void Automation::SetCookie(int tab_id,
                           const GURL& gurl,
                           const std::string& cookie,
                           bool* success) {
  TabProxy* tab = GetTabById(tab_id);
  if (!tab) {
    *success = false;
    return;
  }
  *success = tab->SetCookie(gurl, cookie);
}

void Automation::GetTabIds(std::vector<int>* tab_ids,
                           bool* success) {
  *success = false;
  int browser_count = 0;
  if (!automation()->GetBrowserWindowCount(&browser_count)) {
    LOG(ERROR) << "Failed to get browser window count";
    return;
  }
  TabIdMap tab_id_map;
  for (int browser_index = 0; browser_index < browser_count; ++browser_index) {
    scoped_refptr<BrowserProxy> browser =
        automation()->GetBrowserWindow(browser_index);
    if (!browser.get())
      continue;
    int tab_count = 0;
    if (!browser->GetTabCount(&tab_count))
      continue;

    for (int tab_index = 0; tab_index < tab_count; ++tab_index) {
      scoped_refptr<TabProxy> tab = browser->GetTab(tab_index);
      if (!tab.get())
        continue;
      tab_ids->push_back(tab->handle());
      tab_id_map.insert(std::make_pair(tab->handle(), tab));
    }
  }

  tab_id_map_ = tab_id_map;
  *success = true;
}

void Automation::DoesTabExist(int tab_id, bool* does_exist) {
  TabProxy* tab = GetTabById(tab_id);
  *does_exist = tab && tab->is_valid();
}

void Automation::CloseTab(int tab_id, bool* success) {
  TabProxy* tab = GetTabById(tab_id);
  if (!tab) {
    *success = false;
    return;
  }
  *success = tab->Close(true);
}

TabProxy* Automation::GetTabById(int tab_id) {
  TabIdMap::const_iterator iter = tab_id_map_.find(tab_id);
  if (iter != tab_id_map_.end()) {
    return iter->second.get();
  }
  return NULL;
}

}  // namespace webdriver
