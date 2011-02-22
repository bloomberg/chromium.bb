// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/automation.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/proxy_launcher.h"
#include "chrome/test/automation/tab_proxy.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/point.h"

#if defined(OS_WIN)
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#endif

namespace {

// Gets the path to the default Chrome executable directory. Returns true on
// success.
bool GetDefaultChromeExeDir(FilePath* browser_directory) {
  FilePath build_dir;
  if (PathService::Get(chrome::DIR_APP, &build_dir) &&
      file_util::DirectoryExists(build_dir)) {
    *browser_directory = build_dir;
    return true;
  }
#if defined(OS_WIN)
  // Check the App Paths registry key.
  const wchar_t kSubKey[] =
      L"\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\chrome.exe";
  base::win::RegKey key(HKEY_CURRENT_USER, kSubKey, KEY_READ);
  std::wstring path;
  if (key.ReadValue(L"path", &path) == ERROR_SUCCESS &&
      file_util::DirectoryExists(FilePath(path))) {
    *browser_directory = FilePath(path);
    return true;
  }
  base::win::RegKey sys_key(HKEY_LOCAL_MACHINE, kSubKey, KEY_READ);
  if (sys_key.ReadValue(L"path", &path) == ERROR_SUCCESS &&
      file_util::DirectoryExists(FilePath(path))) {
    *browser_directory = FilePath(path);
    return true;
  }

  // Check the default location for Chrome on the system.
  scoped_ptr<base::Environment> env(base::Environment::Create());
  std::string home_dir;
  if (!env->GetVar("userprofile", &home_dir))
    return false;
  FilePath default_location(UTF8ToWide(home_dir));
  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    default_location = default_location.Append(
        L"Local Settings\\Application Data");
  } else {
    default_location = default_location.Append(L"AppData\\Local");
  }
  default_location.Append(L"Google\\Chrome\\Application");
  if (file_util::DirectoryExists(default_location)) {
    *browser_directory = default_location;
    return true;
  }
#elif defined(OS_MACOSX)
  FilePath app_path(
      "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome");
  if (file_util::DirectoryExists(app_path)) {
    *browser_directory = app_path;
    return true;
  }
#elif defined(OS_LINUX)
  FilePath google_chrome("/usr/bin/google-chrome");
  if (file_util::PathExists(google_chrome)) {
    FilePath chrome;
    if (file_util::ReadSymbolicLink(google_chrome, &chrome)) {
      chrome = chrome.DirName();
      if (file_util::DirectoryExists(chrome)) {
        *browser_directory = chrome;
        return true;
      }
    }
  }
#endif
  return false;
}

}  // namespace

namespace webdriver {

WebKeyEvent::WebKeyEvent(automation::KeyEventTypes type,
                         ui::KeyboardCode key_code,
                         const std::string& unmodified_text,
                         const std::string& modified_text,
                         int modifiers)
    : type(type),
      key_code(key_code),
      unmodified_text(unmodified_text),
      modified_text(modified_text),
      modifiers(modifiers) {}

Automation::Automation() {}

Automation::~Automation() {}

void Automation::Init(const FilePath& user_browser_dir, bool* success) {
  FilePath browser_dir = user_browser_dir;
  if (browser_dir.empty() && !GetDefaultChromeExeDir(&browser_dir)) {
    LOG(ERROR) << "Could not locate Chrome application directory";
    *success = false;
    return;
  }

  CommandLine args(CommandLine::NO_PROGRAM);
  args.AppendSwitch(switches::kDomAutomationController);
  args.AppendSwitch(switches::kFullMemoryCrashReport);
  args.AppendSwitch(switches::kNoDefaultBrowserCheck);
  args.AppendSwitch(switches::kNoFirstRun);
  args.AppendSwitchASCII(switches::kTestType, "webdriver");

  launcher_.reset(new AnonymousProxyLauncher(false));
  ProxyLauncher::LaunchState launch_props = {
      false,  // clear_profile
      FilePath(),  // template_user_data
      ProxyLauncher::DEFAULT_THEME,
      browser_dir,
      args,
      true,  // include_testing_id
      true   // show_window
  };
  launcher_->LaunchBrowserAndServer(launch_props, true);
  *success = launcher_->IsBrowserRunning();
}

void Automation::Terminate() {
  launcher_->QuitBrowser();
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

void Automation::MouseMove(int tab_id,
                           const gfx::Point& p,
                           bool* success) {
  std::string reply;
  DictionaryValue dict;

  dict.SetString("command", "WebkitMouseMove");
  dict.SetInteger("x", p.x());
  dict.SetInteger("y", p.y());

  *success = SendJSONRequest(tab_id, dict, &reply);
  if (!*success) {
    LOG(ERROR) << "Could not send mouse event. Reply: " << reply;
  }
}

void Automation::MouseClick(int tab_id,
                            const gfx::Point& p,
                            int flag,
                            bool* success) {
  std::string reply;
  DictionaryValue dict;

  dict.SetString("command", "WebkitMouseClick");
  dict.SetInteger("button_flags", flag);
  dict.SetInteger("x", p.x());
  dict.SetInteger("y", p.y());

  *success = SendJSONRequest(tab_id, dict, &reply);
  if (!*success) {
    LOG(ERROR) << "Could not send mouse event. Reply: " << reply;
  }
}

void Automation::MouseDrag(int tab_id,
                           const gfx::Point& start,
                           const gfx::Point& end,
                           bool* success) {
  std::string reply;
  DictionaryValue dict;

  dict.SetString("command", "WebkitMouseDrag");
  dict.SetInteger("start_x", start.x());
  dict.SetInteger("start_y", start.y());
  dict.SetInteger("end_x", end.x());
  dict.SetInteger("end_y", end.y());

  *success = SendJSONRequest(tab_id, dict, &reply);
  if (!*success) {
    LOG(ERROR) << "Could not send mouse event. Reply: " << reply;
  }
}

void Automation::SendWebKeyEvent(int tab_id,
                                 const WebKeyEvent& key_event,
                                 bool* success) {
  std::string reply;
  DictionaryValue dict;

  dict.SetString("command", "SendKeyEventToActiveTab");
  dict.SetInteger("type", key_event.type);
  dict.SetInteger("nativeKeyCode", key_event.key_code);
  dict.SetInteger("windowsKeyCode", key_event.key_code);
  dict.SetString("unmodifiedText", key_event.unmodified_text);
  dict.SetString("text", key_event.modified_text);
  dict.SetInteger("modifiers", key_event.modifiers);
  dict.SetBoolean("isSystemKey", false);

  *success = SendJSONRequest(tab_id, dict, &reply);
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
  if (!launcher_->automation()->GetBrowserWindowCount(&browser_count)) {
    LOG(ERROR) << "Failed to get browser window count";
    return;
  }
  TabIdMap tab_id_map;
  for (int browser_index = 0; browser_index < browser_count; ++browser_index) {
    scoped_refptr<BrowserProxy> browser =
        launcher_->automation()->GetBrowserWindow(browser_index);
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

void Automation::GetVersion(std::string* version) {
  *version = launcher_->automation()->server_version();
}

TabProxy* Automation::GetTabById(int tab_id) {
  TabIdMap::const_iterator iter = tab_id_map_.find(tab_id);
  if (iter != tab_id_map_.end()) {
    return iter->second.get();
  }
  return NULL;
}

bool Automation::SendJSONRequest(int tab_id,
                                 const DictionaryValue& dict,
                                 std::string* reply) {
  std::string request;

  base::JSONWriter::Write(&dict, false, &request);
  TabProxy* tab = GetTabById(tab_id);
  if (!tab) {
    LOG(ERROR) << "No such tab";
    return false;
  }

  int tab_index = 0;
  if (!tab->GetTabIndex(&tab_index)) {
    LOG(ERROR) << "Could not get tab index";
    return false;
  }

  scoped_refptr<BrowserProxy> browser = tab->GetParentBrowser();
  if (!browser.get()) {
    LOG(ERROR) << "Could not get parent browser of tab";
    return false;
  }

  if (!browser->ActivateTab(tab_index)) {
    LOG(ERROR) << "Could not activate tab";
    return false;
  }

  return browser->SendJSONRequest(request, reply);
}

}  // namespace webdriver
