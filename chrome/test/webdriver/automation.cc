// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/automation.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/automation_constants.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/automation_json_requests.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/proxy_launcher.h"
#include "chrome/test/webdriver/frame_path.h"
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
  std::vector<FilePath> locations;
  // Add the directory which this module resides in.
  FilePath module_dir;
  if (PathService::Get(base::DIR_MODULE, &module_dir))
    locations.push_back(module_dir);

#if defined(OS_WIN)
  // Add the App Paths registry key location.
  const wchar_t kSubKey[] =
      L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\chrome.exe";
  base::win::RegKey key(HKEY_CURRENT_USER, kSubKey, KEY_READ);
  std::wstring path;
  if (key.ReadValue(L"path", &path) == ERROR_SUCCESS)
    locations.push_back(FilePath(path));
  base::win::RegKey sys_key(HKEY_LOCAL_MACHINE, kSubKey, KEY_READ);
  if (sys_key.ReadValue(L"path", &path) == ERROR_SUCCESS)
    locations.push_back(FilePath(path));

  // Add the user-level location for Chrome.
  FilePath app_from_google(L"Google\\Chrome\\Application");
  scoped_ptr<base::Environment> env(base::Environment::Create());
  std::string home_dir;
  if (env->GetVar("userprofile", &home_dir)) {
    FilePath default_location(UTF8ToWide(home_dir));
    if (base::win::GetVersion() < base::win::VERSION_VISTA) {
      default_location = default_location.Append(
          L"Local Settings\\Application Data");
    } else {
      default_location = default_location.Append(L"AppData\\Local");
    }
    locations.push_back(default_location.Append(app_from_google));
  }

  // Add the system-level location for Chrome.
  std::string program_dir;
  if (env->GetVar("ProgramFiles", &program_dir)) {
    locations.push_back(FilePath(UTF8ToWide(program_dir))
        .Append(app_from_google));
  }
  if (env->GetVar("ProgramFiles(x86)", &program_dir)) {
    locations.push_back(FilePath(UTF8ToWide(program_dir))
        .Append(app_from_google));
  }
#elif defined(OS_MACOSX)
  locations.push_back(FilePath(
      "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"));
#elif defined(OS_LINUX)
  // Proxy launcher doesn't check for google-chrome, only chrome.
  FilePath chrome_sym_link("/usr/bin/google-chrome");
  if (file_util::PathExists(chrome_sym_link)) {
    FilePath chrome;
    if (file_util::ReadSymbolicLink(chrome_sym_link, &chrome)) {
      locations.push_back(chrome.DirName());
    }
  }
#endif

  // Add the current directory.
  FilePath current_dir;
  if (file_util::GetCurrentDirectory(&current_dir))
    locations.push_back(current_dir);

  // Determine the default directory.
  for (size_t i = 0; i < locations.size(); ++i) {
    if (file_util::PathExists(
            locations[i].Append(chrome::kBrowserProcessExecutablePath))) {
      *browser_directory = locations[i];
      return true;
    }
  }
  return false;
}

}  // namespace

namespace webdriver {

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
  args.AppendSwitch(switches::kDisableHangMonitor);
  args.AppendSwitch(switches::kDisablePromptOnRepost);
  args.AppendSwitch(switches::kDomAutomationController);
  args.AppendSwitch(switches::kFullMemoryCrashReport);
  args.AppendSwitchASCII(switches::kHomePage, chrome::kAboutBlankURL);
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
                               const FramePath& frame_path,
                               const std::string& script,
                               std::string* result,
                               bool* success) {
  int windex = 0, tab_index = 0;
  if (!GetIndicesForTab(tab_id, &windex, &tab_index)) {
    *success = false;
    return;
  }

  Value* unscoped_value;
  if (!SendExecuteJavascriptJSONRequest(automation(), windex, tab_index,
                                        frame_path.value(), script,
                                        &unscoped_value)) {
    *success = false;
    return;
  }
  scoped_ptr<Value> value(unscoped_value);
  *success = value->GetAsString(result);
}

void Automation::MouseMove(int tab_id,
                           const gfx::Point& p,
                           bool* success) {
  int windex = 0, tab_index = 0;
  if (!GetIndicesForTab(tab_id, &windex, &tab_index)) {
    *success = false;
    return;
  }

  *success = SendMouseMoveJSONRequest(
      automation(), windex, tab_index, p.x(), p.y());
}

void Automation::MouseClick(int tab_id,
                            const gfx::Point& p,
                            automation::MouseButton button,
                            bool* success) {
  int windex = 0, tab_index = 0;
  if (!GetIndicesForTab(tab_id, &windex, &tab_index)) {
    *success = false;
    return;
  }

  *success = SendMouseClickJSONRequest(
      automation(), windex, tab_index, button, p.x(), p.y());
}

void Automation::MouseDrag(int tab_id,
                           const gfx::Point& start,
                           const gfx::Point& end,
                           bool* success) {
  int windex = 0, tab_index = 0;
  if (!GetIndicesForTab(tab_id, &windex, &tab_index)) {
    *success = false;
    return;
  }

  *success = SendMouseDragJSONRequest(
      automation(), windex, tab_index, start.x(), start.y(), end.x(), end.y());
}

void Automation::SendWebKeyEvent(int tab_id,
                                 const WebKeyEvent& key_event,
                                 bool* success) {
  int windex = 0, tab_index = 0;
  if (!GetIndicesForTab(tab_id, &windex, &tab_index)) {
    *success = false;
    return;
  }

  *success = SendWebKeyEventJSONRequest(
      automation(), windex, tab_index, key_event);
}

void Automation::NavigateToURL(int tab_id,
                               const std::string& url,
                               bool* success) {
  int browser_index = 0, tab_index = 0;
  if (!GetIndicesForTab(tab_id, &browser_index, &tab_index)) {
    *success = false;
    return;
  }

  AutomationMsg_NavigationResponseValues navigate_response;
  if (!SendNavigateToURLJSONRequest(automation(), browser_index, tab_index,
                                    GURL(url), 1, &navigate_response)) {
    *success = false;
    return;
  }
  *success = navigate_response != AUTOMATION_MSG_NAVIGATION_ERROR;
}

void Automation::GoForward(int tab_id, bool* success) {
  int windex = 0, tab_index = 0;
  if (!GetIndicesForTab(tab_id, &windex, &tab_index)) {
    *success = false;
    return;
  }

  *success = SendGoForwardJSONRequest(automation(), windex, tab_index);
}

void Automation::GoBack(int tab_id, bool* success) {
  int windex = 0, tab_index = 0;
  if (!GetIndicesForTab(tab_id, &windex, &tab_index)) {
    *success = false;
    return;
  }

  *success = SendGoBackJSONRequest(automation(), windex, tab_index);
}

void Automation::Reload(int tab_id, bool* success) {
  int windex = 0, tab_index = 0;
  if (!GetIndicesForTab(tab_id, &windex, &tab_index)) {
    *success = false;
    return;
  }

  *success = SendReloadJSONRequest(automation(), windex, tab_index);
}

void Automation::GetURL(int tab_id,
                        std::string* url,
                        bool* success) {
  int windex = 0, tab_index = 0;
  if (!GetIndicesForTab(tab_id, &windex, &tab_index)) {
    *success = false;
    return;
  }

  *success = SendGetTabURLJSONRequest(automation(), windex, tab_index, url);
}

void Automation::GetGURL(int tab_id,
                         GURL* gurl,
                         bool* success) {
  std::string url;
  GetURL(tab_id, &url, success);
  if (*success)
    *gurl = GURL(url);
}

void Automation::GetTabTitle(int tab_id,
                             std::string* tab_title,
                             bool* success) {
  int windex = 0, tab_index = 0;
  if (!GetIndicesForTab(tab_id, &windex, &tab_index)) {
    *success = false;
    return;
  }

  *success = SendGetTabTitleJSONRequest(
      automation(), windex, tab_index, tab_title);
}

void Automation::GetCookies(int tab_id,
                            const GURL& gurl,
                            std::string* cookies,
                            bool* success) {
  int windex = 0, tab_index = 0;
  if (!GetIndicesForTab(tab_id, &windex, &tab_index)) {
    *success = false;
    return;
  }

  *success = SendGetCookiesJSONRequest(
      automation(), windex, gurl.possibly_invalid_spec(), cookies);
}

void Automation::DeleteCookie(int tab_id,
                              const GURL& gurl,
                              const std::string& cookie_name,
                              bool* success) {
  int windex = 0, tab_index = 0;
  if (!GetIndicesForTab(tab_id, &windex, &tab_index)) {
    *success = false;
    return;
  }

  *success = SendDeleteCookieJSONRequest(
      automation(),
      windex,
      gurl.possibly_invalid_spec(),
      cookie_name);
}

void Automation::SetCookie(int tab_id,
                           const GURL& gurl,
                           const std::string& cookie,
                           bool* success) {
  int windex = 0, tab_index = 0;
  if (!GetIndicesForTab(tab_id, &windex, &tab_index)) {
    *success = false;
    return;
  }

  *success = SendSetCookieJSONRequest(
      automation(),
      windex,
      gurl.possibly_invalid_spec(),
      cookie);
}

void Automation::GetTabIds(std::vector<int>* tab_ids,
                           bool* success) {
  *success = SendGetTabIdsJSONRequest(automation(), tab_ids);
}

void Automation::DoesTabExist(int tab_id, bool* does_exist, bool* success) {
  *success = SendIsTabIdValidJSONRequest(automation(), tab_id, does_exist);
}

void Automation::CloseTab(int tab_id, bool* success) {
  int windex = 0, tab_index = 0;
  if (!GetIndicesForTab(tab_id, &windex, &tab_index)) {
    *success = false;
    return;
  }

  *success = SendCloseTabJSONRequest(automation(), windex, tab_index);
}

void Automation::GetVersion(std::string* version) {
  *version = automation()->server_version();
}

void Automation::WaitForAllTabsToStopLoading(bool* success) {
  *success = SendWaitForAllTabsToStopLoadingJSONRequest(automation());
}

AutomationProxy* Automation::automation() const {
  return launcher_->automation();
}

bool Automation::GetIndicesForTab(
    int tab_id, int* browser_index, int* tab_index) {
  if (!SendGetIndicesFromTabIdJSONRequest(automation(), tab_id,
                                          browser_index, tab_index)) {
    LOG(ERROR) << "Could not get browser and tab indices for WebDriver tab id";
    return false;
  }
  return true;
}

}  // namespace webdriver
