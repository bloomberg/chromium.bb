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
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
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
#include "chrome/test/webdriver/webdriver_error.h"
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
                      Error** error) {
  FilePath browser_exe;
  if (!GetDefaultChromeExe(&browser_exe)) {
    *error = new Error(kUnknownError, "Could not find default Chrome binary");
    return;
  }

  InitWithBrowserPath(browser_exe, options, error);
}

void Automation::InitWithBrowserPath(const FilePath& browser_exe,
                                     const CommandLine& options,
                                     Error** error) {
  if (!file_util::PathExists(browser_exe)) {
    std::string message = base::StringPrintf(
        "Could not find Chrome binary at: %" PRFilePath,
        browser_exe.value().c_str());
    *error = new Error(kUnknownError, message);
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
    *error = new Error(
        kUnknownError,
        "Unable to either launch or connect to Chrome. Please check that "
            "ChromeDriver is up-to-date");
    return;
  }

  bool has_automation_version = false;
  *error = CompareVersion(730, 0, &has_automation_version);
  if (*error)
    return;
  if (has_automation_version) {
    int version = 0;
    std::string error_msg;
    if (!SendGetChromeDriverAutomationVersion(
            automation(), &version, &error_msg)) {
      *error = CreateChromeError(error_msg);
      return;
    }
    if (version > automation::kChromeDriverAutomationVersion) {
      *error = new Error(
          kUnknownError,
          "ChromeDriver is not compatible with this version of Chrome.");
      return;
    }
  }
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
                               Error** error) {
  int windex = 0, tab_index = 0;
  *error = GetIndicesForTab(tab_id, &windex, &tab_index);
  if (*error)
    return;

  Value* unscoped_value;
  std::string error_msg;
  if (!SendExecuteJavascriptJSONRequest(automation(), windex, tab_index,
                                        frame_path.value(), script,
                                        &unscoped_value, &error_msg)) {
    *error = CreateChromeError(error_msg);
    return;
  }
  scoped_ptr<Value> value(unscoped_value);
  if (!value->GetAsString(result))
    *error = CreateChromeError("Execute script did not return string");
}

void Automation::MouseMove(int tab_id,
                           const gfx::Point& p,
                           Error** error) {
  int windex = 0, tab_index = 0;
  *error = GetIndicesForTab(tab_id, &windex, &tab_index);
  if (*error)
    return;

  std::string error_msg;
  if (!SendMouseMoveJSONRequest(
          automation(), windex, tab_index, p.x(), p.y(), &error_msg)) {
    *error = CreateChromeError(error_msg);
  }
}

void Automation::MouseClick(int tab_id,
                            const gfx::Point& p,
                            automation::MouseButton button,
                            Error** error) {
  int windex = 0, tab_index = 0;
  *error = GetIndicesForTab(tab_id, &windex, &tab_index);
  if (*error)
    return;

  std::string error_msg;
  if (!SendMouseClickJSONRequest(
          automation(), windex, tab_index, button, p.x(), p.y(), &error_msg)) {
    *error = CreateChromeError(error_msg);
  }
}

void Automation::MouseDrag(int tab_id,
                           const gfx::Point& start,
                           const gfx::Point& end,
                           Error** error) {
  int windex = 0, tab_index = 0;
  *error = GetIndicesForTab(tab_id, &windex, &tab_index);
  if (*error)
    return;

  std::string error_msg;
  if (!SendMouseDragJSONRequest(automation(), windex, tab_index, start.x(),
                                start.y(), end.x(), end.y(), &error_msg)) {
    *error = CreateChromeError(error_msg);
  }
}

void Automation::MouseButtonUp(int tab_id,
                               const gfx::Point& p,
                               Error** error) {
  *error = CheckAdvancedInteractionsSupported();
  if (*error)
    return;

  int windex = 0, tab_index = 0;
  *error = GetIndicesForTab(tab_id, &windex, &tab_index);
  if (*error)
    return;

  std::string error_msg;
  if (!SendMouseButtonUpJSONRequest(
          automation(), windex, tab_index, p.x(), p.y(), &error_msg)) {
    *error = CreateChromeError(error_msg);
  }
}

void Automation::MouseButtonDown(int tab_id,
                                 const gfx::Point& p,
                                 Error** error) {
  *error = CheckAdvancedInteractionsSupported();
  if (*error)
    return;

  int windex = 0, tab_index = 0;
  *error = GetIndicesForTab(tab_id, &windex, &tab_index);
  if (*error)
    return;

  std::string error_msg;
  if (!SendMouseButtonDownJSONRequest(
          automation(), windex, tab_index, p.x(), p.y(), &error_msg)) {
    *error = CreateChromeError(error_msg);
  }
}

void Automation::MouseDoubleClick(int tab_id,
                                  const gfx::Point& p,
                                  Error** error) {
  *error = CheckAdvancedInteractionsSupported();
  if (*error)
    return;

  int windex = 0, tab_index = 0;
  *error = GetIndicesForTab(tab_id, &windex, &tab_index);
  if (*error)
    return;

  std::string error_msg;
  if (!SendMouseDoubleClickJSONRequest(
          automation(), windex, tab_index, p.x(), p.y(), &error_msg)) {
    *error = CreateChromeError(error_msg);
  }
}

void Automation::SendWebKeyEvent(int tab_id,
                                 const WebKeyEvent& key_event,
                                 Error** error) {
  int windex = 0, tab_index = 0;
  *error = GetIndicesForTab(tab_id, &windex, &tab_index);
  if (*error)
    return;

  std::string error_msg;
  if (!SendWebKeyEventJSONRequest(
          automation(), windex, tab_index, key_event, &error_msg)) {
    *error = CreateChromeError(error_msg);
  }
}

void Automation::SendNativeKeyEvent(int tab_id,
                                    ui::KeyboardCode key_code,
                                    int modifiers,
                                    Error** error) {
  int windex = 0, tab_index = 0;
  *error = GetIndicesForTab(tab_id, &windex, &tab_index);
  if (*error)
    return;

  std::string error_msg;
  if (!SendNativeKeyEventJSONRequest(
         automation(), windex, tab_index, key_code, modifiers, &error_msg)) {
    *error = CreateChromeError(error_msg);
  }
}

void Automation::CaptureEntirePageAsPNG(int tab_id,
                                        const FilePath& path,
                                        Error** error) {
  int windex = 0, tab_index = 0;
  *error = GetIndicesForTab(tab_id, &windex, &tab_index);
  if (*error)
    return;

  std::string error_msg;
  if (!SendCaptureEntirePageJSONRequest(
          automation(), windex, tab_index, path, &error_msg)) {
    *error = CreateChromeError(error_msg);
  }
}

void Automation::NavigateToURL(int tab_id,
                               const std::string& url,
                               Error** error) {
  int windex = 0, tab_index = 0;
  *error = GetIndicesForTab(tab_id, &windex, &tab_index);
  if (*error)
    return;

  AutomationMsg_NavigationResponseValues navigate_response;
  std::string error_msg;
  if (!SendNavigateToURLJSONRequest(automation(), windex, tab_index,
                                    GURL(url), 1, &navigate_response,
                                    &error_msg)) {
    *error = CreateChromeError(error_msg);
    return;
  }
  // TODO(kkania): Do not rely on this enum.
  if (navigate_response == AUTOMATION_MSG_NAVIGATION_ERROR)
    *error = new Error(kUnknownError, "Navigation error occurred");
}

void Automation::GoForward(int tab_id, Error** error) {
  int windex = 0, tab_index = 0;
  *error = GetIndicesForTab(tab_id, &windex, &tab_index);
  if (*error)
    return;

  std::string error_msg;
  if (!SendGoForwardJSONRequest(
          automation(), windex, tab_index, &error_msg)) {
    *error = CreateChromeError(error_msg);
  }
}

void Automation::GoBack(int tab_id, Error** error) {
  int windex = 0, tab_index = 0;
  *error = GetIndicesForTab(tab_id, &windex, &tab_index);
  if (*error)
    return;

  std::string error_msg;
  if (!SendGoBackJSONRequest(automation(), windex, tab_index, &error_msg))
    *error = CreateChromeError(error_msg);
}

void Automation::Reload(int tab_id, Error** error) {
  int windex = 0, tab_index = 0;
  *error = GetIndicesForTab(tab_id, &windex, &tab_index);
  if (*error)
    return;

  std::string error_msg;
  if (!SendReloadJSONRequest(automation(), windex, tab_index, &error_msg))
    *error = CreateChromeError(error_msg);
}

void Automation::GetCookies(const std::string& url,
                            ListValue** cookies,
                            Error** error) {
  std::string error_msg;
  if (!SendGetCookiesJSONRequest(automation(), url, cookies, &error_msg))
    *error = CreateChromeError(error_msg);
}

void Automation::GetCookiesDeprecated(int tab_id,
                                      const GURL& gurl,
                                      std::string* cookies,
                                      bool* success) {
  int windex = 0, tab_index = 0;
  scoped_ptr<Error> error(GetIndicesForTab(tab_id, &windex, &tab_index));
  if (error.get()) {
    *success = false;
    return;
  }

  *success = SendGetCookiesJSONRequestDeprecated(
      automation(), windex, gurl.possibly_invalid_spec(), cookies);
}

void Automation::DeleteCookie(const std::string& url,
                              const std::string& cookie_name,
                              Error** error) {
  std::string error_msg;
  if (!SendDeleteCookieJSONRequest(
          automation(), url, cookie_name, &error_msg)) {
    *error = CreateChromeError(error_msg);
  }
}

void Automation::DeleteCookieDeprecated(int tab_id,
                                        const GURL& gurl,
                                        const std::string& cookie_name,
                                        bool* success) {
  int windex = 0, tab_index = 0;
  scoped_ptr<Error> error(GetIndicesForTab(tab_id, &windex, &tab_index));
  if (error.get()) {
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
                           Error** error) {
  std::string error_msg;
  if (!SendSetCookieJSONRequest(automation(), url, cookie_dict, &error_msg))
    *error = CreateChromeError(error_msg);
}

void Automation::SetCookieDeprecated(int tab_id,
                                     const GURL& gurl,
                                     const std::string& cookie,
                                     bool* success) {
  int windex = 0, tab_index = 0;
  scoped_ptr<Error> error(GetIndicesForTab(tab_id, &windex, &tab_index));
  if (error.get()) {
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
                           Error** error) {
  std::string error_msg;
  if (!SendGetTabIdsJSONRequest(automation(), tab_ids, &error_msg))
    *error = CreateChromeError(error_msg);
}

void Automation::DoesTabExist(int tab_id, bool* does_exist, Error** error) {
  std::string error_msg;
  if (!SendIsTabIdValidJSONRequest(
          automation(), tab_id, does_exist, &error_msg)) {
    *error = CreateChromeError(error_msg);
  }
}

void Automation::CloseTab(int tab_id, Error** error) {
  int windex = 0, tab_index = 0;
  *error = GetIndicesForTab(tab_id, &windex, &tab_index);
  if (*error)
    return;

  std::string error_msg;
  if (!SendCloseTabJSONRequest(automation(), windex, tab_index, &error_msg))
    *error = CreateChromeError(error_msg);
}

void Automation::GetAppModalDialogMessage(std::string* message, Error** error) {
  *error = CheckAlertsSupported();
  if (*error)
    return;

  std::string error_msg;
  if (!SendGetAppModalDialogMessageJSONRequest(
          automation(), message, &error_msg)) {
    *error = CreateChromeError(error_msg);
  }
}

void Automation::AcceptOrDismissAppModalDialog(bool accept, Error** error) {
  *error = CheckAlertsSupported();
  if (*error)
    return;

  std::string error_msg;
  if (!SendAcceptOrDismissAppModalDialogJSONRequest(
          automation(), accept, &error_msg)) {
    *error = CreateChromeError(error_msg);
  }
}

void Automation::AcceptPromptAppModalDialog(const std::string& prompt_text,
                                            Error** error) {
  *error = CheckAlertsSupported();
  if (*error)
    return;

  std::string error_msg;
  if (!SendAcceptPromptAppModalDialogJSONRequest(
          automation(), prompt_text, &error_msg)) {
    *error = CreateChromeError(error_msg);
  }
}

void Automation::GetBrowserVersion(std::string* version) {
  *version = automation()->server_version();
}

void Automation::GetChromeDriverAutomationVersion(int* version, Error** error) {
  std::string error_msg;
  if (!SendGetChromeDriverAutomationVersion(automation(), version, &error_msg))
    *error = CreateChromeError(error_msg);
}

void Automation::WaitForAllTabsToStopLoading(Error** error) {
  std::string error_msg;
  if (!SendWaitForAllTabsToStopLoadingJSONRequest(automation(), &error_msg))
    *error = CreateChromeError(error_msg);
}

AutomationProxy* Automation::automation() const {
  return launcher_->automation();
}

Error* Automation::GetIndicesForTab(
    int tab_id, int* browser_index, int* tab_index) {
  std::string error_msg;
  if (!SendGetIndicesFromTabIdJSONRequest(
          automation(), tab_id, browser_index, tab_index, &error_msg)) {
    return CreateChromeError(error_msg);
  }
  return NULL;
}

Error* Automation::CreateChromeError(const std::string& message) {
  return new Error(kUnknownError, "Internal Chrome error: " + message);
}

Error* Automation::CompareVersion(int client_build_no,
                                  int client_patch_no,
                                  bool* is_newer_or_equal) {
  std::string version = automation()->server_version();
  std::vector<std::string> split_version;
  base::SplitString(version, '.', &split_version);
  if (split_version.size() != 4) {
    return new Error(
        kUnknownError, "Browser version has unrecognized format: " + version);
  }
  int build_no, patch_no;
  if (!base::StringToInt(split_version[2], &build_no) ||
      !base::StringToInt(split_version[3], &patch_no)) {
    return new Error(
        kUnknownError, "Browser version has unrecognized format: " + version);
  }
  if (build_no < client_build_no)
    *is_newer_or_equal = false;
  else if (build_no > client_build_no)
    *is_newer_or_equal = true;
  else
    *is_newer_or_equal = patch_no >= client_patch_no;
  return NULL;
}

Error* Automation::CheckVersion(int client_build_no,
                                int client_patch_no,
                                const std::string& error_msg) {
  bool version_is_ok = false;
  Error* error = CompareVersion(
      client_build_no, client_patch_no, &version_is_ok);
  if (error)
    return error;
  if (!version_is_ok)
    return new Error(kUnknownError, error_msg);
  return NULL;
}

Error* Automation::CheckAlertsSupported() {
  return CheckVersion(
      768, 0, "Alerts are not supported for this version of Chrome");
}

Error* Automation::CheckAdvancedInteractionsSupported() {
  const char* message =
      "Advanced user interactions are not supported for this version of Chrome";
  return CheckVersion(750, 0, message);
}

}  // namespace webdriver
