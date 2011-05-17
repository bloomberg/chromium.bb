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

// Gets the path to the default Chrome executable. Returns true on success.
bool GetDefaultChromeExe(FilePath* browser_exe) {
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
  locations.push_back(FilePath("/Applications"));
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
    FilePath path = locations[i].Append(chrome::kBrowserProcessExecutablePath);
    if (file_util::PathExists(path)) {
      *browser_exe = path;
      return true;
    }
  }
  return false;
}

}  // namespace

namespace webdriver {

Automation::Automation() {}

Automation::~Automation() {}

void Automation::Init(const CommandLine& options,
                      ErrorCode* code) {
  FilePath browser_exe;
  if (!GetDefaultChromeExe(&browser_exe)) {
    *code = kBrowserCouldNotBeFound;
    return;
  }

  InitWithBrowserPath(browser_exe, options, code);
}

void Automation::InitWithBrowserPath(const FilePath& browser_exe,
                                     const CommandLine& options,
                                     ErrorCode* code) {
  if (!file_util::PathExists(browser_exe)) {
    *code = kBrowserCouldNotBeFound;
    return;
  }

  CommandLine command(browser_exe);
  command.AppendSwitch(switches::kDisableHangMonitor);
  command.AppendSwitch(switches::kDisablePromptOnRepost);
  command.AppendSwitch(switches::kDomAutomationController);
  command.AppendSwitch(switches::kFullMemoryCrashReport);
  command.AppendSwitchASCII(switches::kHomePage, chrome::kAboutBlankURL);
  command.AppendSwitch(switches::kNoDefaultBrowserCheck);
  command.AppendSwitch(switches::kNoFirstRun);
  command.AppendSwitchASCII(switches::kTestType, "webdriver");

  command.AppendArguments(options, false);

  launcher_.reset(new AnonymousProxyLauncher(false));
  ProxyLauncher::LaunchState launch_props = {
      false,  // clear_profile
      FilePath(),  // template_user_data
      ProxyLauncher::DEFAULT_THEME,
      command,
      true,  // include_testing_id
      true   // show_window
  };

  if (!launcher_->LaunchBrowserAndServer(launch_props, true)) {
    *code = kBrowserFailedToStart;
    return;
  }

  int version = 0;
  if (!SendGetChromeDriverAutomationVersion(automation(), &version) ||
      version > automation::kChromeDriverAutomationVersion) {
    *code = kIncompatibleBrowserVersion;
    return;
  }
  *code = kSuccess;
}

void Automation::Terminate() {
  if (launcher_.get() && launcher_->process() != base::kNullProcessHandle) {
    launcher_->QuitBrowser();
  }
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

void Automation::MouseButtonUp(int tab_id,
                               const gfx::Point& p,
                               bool* success) {
  int windex = 0, tab_index = 0;
  if (!GetIndicesForTab(tab_id, &windex, &tab_index)) {
    *success = false;
    return;
  }

  *success = SendMouseButtonUpJSONRequest(
      automation(), windex, tab_index, p.x(), p.y());
}

void Automation::MouseButtonDown(int tab_id,
                                 const gfx::Point& p,
                                 bool* success) {
  int windex = 0, tab_index = 0;
  if (!GetIndicesForTab(tab_id, &windex, &tab_index)) {
    *success = false;
    return;
  }

  *success = SendMouseButtonDownJSONRequest(
      automation(), windex, tab_index, p.x(), p.y());
}

void Automation::MouseDoubleClick(int tab_id,
                                  const gfx::Point& p,
                                  bool* success) {
  int windex = 0, tab_index = 0;
  if (!GetIndicesForTab(tab_id, &windex, &tab_index)) {
    *success = false;
    return;
  }

  *success = SendMouseDoubleClickJSONRequest(
      automation(), windex, tab_index, p.x(), p.y());
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

void Automation::SendNativeKeyEvent(int tab_id,
                                    ui::KeyboardCode key_code,
                                    int modifiers,
                                    bool* success) {
  int windex = 0, tab_index = 0;
  if (!GetIndicesForTab(tab_id, &windex, &tab_index)) {
    *success = false;
    return;
  }
  *success = SendNativeKeyEventJSONRequest(
      automation(), windex, tab_index, key_code, modifiers);
}

void Automation::CaptureEntirePageAsPNG(int tab_id,
                                        const FilePath& path,
                                        bool* success) {
  int windex = 0, tab_index = 0;
  if (!GetIndicesForTab(tab_id, &windex, &tab_index)) {
    *success = false;
    return;
  }

  *success = SendCaptureEntirePageJSONRequest(
      automation(), windex, tab_index, path);
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

void Automation::GetCookies(const std::string& url,
                            ListValue** cookies,
                            bool* success) {
  *success = SendGetCookiesJSONRequest(automation(), url, cookies);
}

void Automation::GetCookiesDeprecated(int tab_id,
                                      const GURL& gurl,
                                      std::string* cookies,
                                      bool* success) {
  int windex = 0, tab_index = 0;
  if (!GetIndicesForTab(tab_id, &windex, &tab_index)) {
    *success = false;
    return;
  }

  *success = SendGetCookiesJSONRequestDeprecated(
      automation(), windex, gurl.possibly_invalid_spec(), cookies);
}

void Automation::DeleteCookie(const std::string& url,
                              const std::string& cookie_name,
                              bool* success) {
  *success = SendDeleteCookieJSONRequest(automation(), url, cookie_name);
}

void Automation::DeleteCookieDeprecated(int tab_id,
                                        const GURL& gurl,
                                        const std::string& cookie_name,
                                        bool* success) {
  int windex = 0, tab_index = 0;
  if (!GetIndicesForTab(tab_id, &windex, &tab_index)) {
    *success = false;
    return;
  }

  *success = SendDeleteCookieJSONRequestDeprecated(
      automation(),
      windex,
      gurl.possibly_invalid_spec(),
      cookie_name);
}

void Automation::SetCookie(const std::string& url,
                           DictionaryValue* cookie_dict,
                           bool* success) {
  *success = SendSetCookieJSONRequest(automation(), url, cookie_dict);
}

void Automation::SetCookieDeprecated(int tab_id,
                                     const GURL& gurl,
                                     const std::string& cookie,
                                     bool* success) {
  int windex = 0, tab_index = 0;
  if (!GetIndicesForTab(tab_id, &windex, &tab_index)) {
    *success = false;
    return;
  }

  *success = SendSetCookieJSONRequestDeprecated(
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

void Automation::GetAppModalDialogMessage(std::string* message, bool* success) {
  *success = SendGetAppModalDialogMessageJSONRequest(automation(), message);
}

void Automation::AcceptOrDismissAppModalDialog(bool accept, bool* success) {
  *success = SendAcceptOrDismissAppModalDialogJSONRequest(
      automation(), accept);
}

void Automation::AcceptPromptAppModalDialog(const std::string& prompt_text,
                                            bool* success) {
  *success = SendAcceptPromptAppModalDialogJSONRequest(
      automation(), prompt_text);
}

void Automation::GetBrowserVersion(std::string* version) {
  *version = automation()->server_version();
}

void Automation::GetChromeDriverAutomationVersion(int* version, bool* success) {
  *success = SendGetChromeDriverAutomationVersion(automation(), version);
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
