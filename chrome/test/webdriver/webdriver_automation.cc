// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/webdriver_automation.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/base_paths.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/environment.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/automation_constants.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/automation_json_requests.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/extension_proxy.h"
#include "chrome/test/automation/proxy_launcher.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/webdriver/frame_path.h"
#include "chrome/test/webdriver/webdriver_basic_types.h"
#include "chrome/test/webdriver/webdriver_error.h"
#include "chrome/test/webdriver/webdriver_util.h"

#if defined(OS_WIN)
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#endif

namespace {

// Iterates through each browser executable path, and checks if the path exists
// in any of the given locations. If found, returns true and sets |browser_exe|.
bool CheckForChromeExe(const std::vector<FilePath>& browser_exes,
                       const std::vector<FilePath>& locations,
                       FilePath* browser_exe) {
  for (size_t i = 0; i < browser_exes.size(); ++i) {
    for (size_t j = 0; j < locations.size(); ++j) {
      FilePath path = locations[j].Append(browser_exes[i]);
      if (file_util::PathExists(path)) {
        *browser_exe = path;
        return true;
      }
    }
  }
  return false;
}

// Gets the path to the default Chrome executable. Returns true on success.
bool GetDefaultChromeExe(FilePath* browser_exe) {
  // Instead of using chrome constants, we hardcode these constants here so
  // that we can locate chrome or chromium regardless of the branding
  // chromedriver is built with. It may be argued that then we need to keep
  // these in sync with chrome constants. However, if chrome constants changes,
  // we need to look for the previous and new versions to support some
  // backwards compatibility.
#if defined(OS_WIN)
  FilePath browser_exes_array[] = {
      FilePath(L"chrome.exe")
  };
#elif defined(OS_MACOSX)
  FilePath browser_exes_array[] = {
      FilePath("Google Chrome.app/Contents/MacOS/Google Chrome"),
      FilePath("Chromium.app/Contents/MacOS/Chromium")
  };
#elif defined(OS_LINUX)
  FilePath browser_exes_array[] = {
      FilePath("google-chrome"),
      FilePath("chrome"),
      FilePath("chromium"),
      FilePath("chromium-browser")
  };
#endif
  std::vector<FilePath> browser_exes(
      browser_exes_array, browser_exes_array + arraysize(browser_exes_array));

  // Step 1: Check the directory this module resides in. This is done
  // before all else so that the tests will pickup the built chrome.
  FilePath module_dir;
  if (PathService::Get(base::DIR_MODULE, &module_dir)) {
    for (size_t j = 0; j < browser_exes.size(); ++j) {
      FilePath path = module_dir.Append(browser_exes[j]);
      if (file_util::PathExists(path)) {
        *browser_exe = path;
        return true;
      }
    }
  }

  // Step 2: Add all possible install locations, in order they should be
  // searched. If a location can only hold a chromium install, add it to
  // |chromium_locations|. Since on some platforms we cannot tell by the binary
  // name whether it is chrome or chromium, we search these locations last.
  // We attempt to run chrome before chromium, if any install can be found.
  std::vector<FilePath> locations;
  std::vector<FilePath> chromium_locations;
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
  FilePath app_from_chromium(L"Chromium\\Application");
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
    chromium_locations.push_back(default_location.Append(app_from_chromium));
  }

  // Add the system-level location for Chrome.
  std::string program_dir;
  if (env->GetVar("ProgramFiles", &program_dir)) {
    locations.push_back(FilePath(UTF8ToWide(program_dir))
        .Append(app_from_google));
    chromium_locations.push_back(FilePath(UTF8ToWide(program_dir))
        .Append(app_from_chromium));
  }
  if (env->GetVar("ProgramFiles(x86)", &program_dir)) {
    locations.push_back(FilePath(UTF8ToWide(program_dir))
        .Append(app_from_google));
    chromium_locations.push_back(FilePath(UTF8ToWide(program_dir))
        .Append(app_from_chromium));
  }
#elif defined(OS_MACOSX)
  std::vector<FilePath> app_dirs;
  webdriver::GetApplicationDirs(&app_dirs);
  locations.insert(locations.end(), app_dirs.begin(), app_dirs.end());
#elif defined(OS_LINUX)
  locations.push_back(FilePath("/opt/google/chrome"));
  locations.push_back(FilePath("/usr/local/bin"));
  locations.push_back(FilePath("/usr/local/sbin"));
  locations.push_back(FilePath("/usr/bin"));
  locations.push_back(FilePath("/usr/sbin"));
  locations.push_back(FilePath("/bin"));
  locations.push_back(FilePath("/sbin"));
#endif

  // Add the current directory.
  FilePath current_dir;
  if (file_util::GetCurrentDirectory(&current_dir))
    locations.push_back(current_dir);

  // Step 3: For each browser exe path, check each location to see if the
  // browser is installed there. Check the chromium locations lastly.
  return CheckForChromeExe(browser_exes, locations, browser_exe) ||
      CheckForChromeExe(browser_exes, chromium_locations, browser_exe);
}

}  // namespace

namespace webdriver {

Automation::BrowserOptions::BrowserOptions()
    : command(CommandLine::NO_PROGRAM),
      detach_process(false) {}

Automation::BrowserOptions::~BrowserOptions() {}

Automation::Automation() {}

Automation::~Automation() {}

void Automation::Init(const BrowserOptions& options, Error** error) {
  // Prepare Chrome's command line.
  CommandLine command(CommandLine::NO_PROGRAM);
  command.AppendSwitch(switches::kDisableHangMonitor);
  command.AppendSwitch(switches::kDisablePromptOnRepost);
  command.AppendSwitch(switches::kDomAutomationController);
  command.AppendSwitch(switches::kFullMemoryCrashReport);
  command.AppendSwitch(switches::kNoDefaultBrowserCheck);
  command.AppendSwitch(switches::kNoFirstRun);
  if (options.detach_process)
    command.AppendSwitch(switches::kAutomationReinitializeOnChannelError);
  if (options.user_data_dir.empty())
    command.AppendSwitchASCII(switches::kHomePage, chrome::kAboutBlankURL);

  command.AppendArguments(options.command, true /* include_program */);

  // Find the Chrome binary.
  if (command.GetProgram().empty()) {
    FilePath browser_exe;
    if (!GetDefaultChromeExe(&browser_exe)) {
      *error = new Error(kUnknownError, "Could not find default Chrome binary");
      return;
    }
    command.SetProgram(browser_exe);
  }
  if (!file_util::PathExists(command.GetProgram())) {
    std::string message = base::StringPrintf(
        "Could not find Chrome binary at: %" PRFilePath,
        command.GetProgram().value().c_str());
    *error = new Error(kUnknownError, message);
    return;
  }
  std::string chrome_details = base::StringPrintf(
      "Using Chrome binary at: %" PRFilePath,
      command.GetProgram().value().c_str());
  LOG(INFO) << chrome_details;

  // Create the ProxyLauncher and launch Chrome.
  // In Chrome 13/14, the only way to detach the browser process is to use a
  // named proxy launcher.
  // TODO(kkania): Remove this when Chrome 15 is stable.
  std::string channel_id = options.channel_id;
  bool launch_browser = false;
  if (options.detach_process) {
    launch_browser = true;
    if (!channel_id.empty()) {
      *error = new Error(
          kUnknownError, "Cannot detach an already running browser process");
      return;
    }
#if defined(OS_WIN)
    channel_id = "chromedriver" + GenerateRandomID();
#elif defined(OS_POSIX)
    FilePath temp_file;
    if (!file_util::CreateTemporaryFile(&temp_file) ||
        !file_util::Delete(temp_file, false /* recursive */)) {
      *error = new Error(kUnknownError, "Could not create temporary filename");
      return;
    }
    channel_id = temp_file.value();
#endif
  }
  if (channel_id.empty()) {
    launcher_.reset(new AnonymousProxyLauncher(false));
  } else {
    LOG(INFO) << "Using named testing interface";
    launcher_.reset(new NamedProxyLauncher(channel_id, launch_browser, false));
  }
  ProxyLauncher::LaunchState launch_props = {
      false,  // clear_profile
      options.user_data_dir,  // template_user_data
      base::Closure(),
      command,
      true,  // include_testing_id
      true   // show_window
  };
  if (!launcher_->InitializeConnection(launch_props, true)) {
    LOG(ERROR) << "Failed to initialize connection";
    *error = new Error(
        kUnknownError,
        "Unable to either launch or connect to Chrome. Please check that "
            "ChromeDriver is up-to-date. " + chrome_details);
    return;
  }

  launcher_->automation()->set_action_timeout_ms(base::kNoTimeout);
  LOG(INFO) << "Chrome launched successfully. Version: "
            << automation()->server_version();

  // Check the version of Chrome is compatible with this ChromeDriver.
  chrome_details += ", version (" + automation()->server_version() + ")";
  int version = 0;
  std::string error_msg;
  if (!SendGetChromeDriverAutomationVersion(
          automation(), &version, &error_msg)) {
    *error = new Error(kUnknownError, error_msg + " " + chrome_details);
    return;
  }
  if (version > automation::kChromeDriverAutomationVersion) {
    *error = new Error(
        kUnknownError,
        "ChromeDriver is not compatible with this version of Chrome. " +
            chrome_details);
    return;
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
    *error = new Error(kUnknownError, error_msg);
    return;
  }
  scoped_ptr<Value> value(unscoped_value);
  if (!value->GetAsString(result))
    *error = new Error(kUnknownError, "Execute script did not return string");
}

void Automation::MouseMove(int tab_id,
                           const Point& p,
                           Error** error) {
  int windex = 0, tab_index = 0;
  *error = GetIndicesForTab(tab_id, &windex, &tab_index);
  if (*error)
    return;

  std::string error_msg;
  if (!SendMouseMoveJSONRequest(
          automation(), windex, tab_index, p.rounded_x(), p.rounded_y(),
          &error_msg)) {
    *error = new Error(kUnknownError, error_msg);
  }
}

void Automation::MouseClick(int tab_id,
                            const Point& p,
                            automation::MouseButton button,
                            Error** error) {
  int windex = 0, tab_index = 0;
  *error = GetIndicesForTab(tab_id, &windex, &tab_index);
  if (*error)
    return;

  std::string error_msg;
  if (!SendMouseClickJSONRequest(
          automation(), windex, tab_index, button, p.rounded_x(),
          p.rounded_y(), &error_msg)) {
    *error = new Error(kUnknownError, error_msg);
  }
}

void Automation::MouseDrag(int tab_id,
                           const Point& start,
                           const Point& end,
                           Error** error) {
  int windex = 0, tab_index = 0;
  *error = GetIndicesForTab(tab_id, &windex, &tab_index);
  if (*error)
    return;

  std::string error_msg;
  if (!SendMouseDragJSONRequest(
          automation(), windex, tab_index, start.rounded_x(), start.rounded_y(),
          end.rounded_x(), end.rounded_y(), &error_msg)) {
    *error = new Error(kUnknownError, error_msg);
  }
}

void Automation::MouseButtonUp(int tab_id,
                               const Point& p,
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
          automation(), windex, tab_index, p.rounded_x(), p.rounded_y(),
          &error_msg)) {
    *error = new Error(kUnknownError, error_msg);
  }
}

void Automation::MouseButtonDown(int tab_id,
                                 const Point& p,
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
          automation(), windex, tab_index, p.rounded_x(), p.rounded_y(),
          &error_msg)) {
    *error = new Error(kUnknownError, error_msg);
  }
}

void Automation::MouseDoubleClick(int tab_id,
                                  const Point& p,
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
          automation(), windex, tab_index, p.rounded_x(), p.rounded_y(),
          &error_msg)) {
    *error = new Error(kUnknownError, error_msg);
  }
}

void Automation::DragAndDropFilePaths(
    int tab_id, const Point& location,
    const std::vector<FilePath::StringType>& paths, Error** error) {
  int windex = 0, tab_index = 0;
  *error = GetIndicesForTab(tab_id, &windex, &tab_index);
  if (*error) {
    return;
  }

  std::string error_msg;
  if (!SendDragAndDropFilePathsJSONRequest(
          automation(), windex, tab_index, location.rounded_x(),
          location.rounded_y(), paths, &error_msg)) {
    *error = new Error(kUnknownError, error_msg);
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
    *error = new Error(kUnknownError, error_msg);
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
    *error = new Error(kUnknownError, error_msg);
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
    *error = new Error(kUnknownError, error_msg);
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
                                    url, 1, &navigate_response,
                                    &error_msg)) {
    *error = new Error(kUnknownError, error_msg);
    return;
  }
  // TODO(kkania): Do not rely on this enum.
  if (navigate_response == AUTOMATION_MSG_NAVIGATION_ERROR)
    *error = new Error(kUnknownError, "Navigation error occurred");
}

void Automation::NavigateToURLAsync(int tab_id,
                                    const std::string& url,
                                    Error** error) {
  int windex = 0, tab_index = 0;
  *error = GetIndicesForTab(tab_id, &windex, &tab_index);
  if (*error)
    return;

  scoped_refptr<BrowserProxy> browser = automation()->GetBrowserWindow(windex);
  if (!browser) {
    *error = new Error(kUnknownError, "Couldn't obtain browser proxy");
    return;
  }
  scoped_refptr<TabProxy> tab = browser->GetTab(tab_index);
  if (!tab) {
    *error = new Error(kUnknownError, "Couldn't obtain tab proxy");
    return;
  }
  if (!tab->NavigateToURLAsync(GURL(url))) {
    *error = new Error(kUnknownError, "Unable to navigate to url");
    return;
  }
}

void Automation::GoForward(int tab_id, Error** error) {
  int windex = 0, tab_index = 0;
  *error = GetIndicesForTab(tab_id, &windex, &tab_index);
  if (*error)
    return;

  std::string error_msg;
  if (!SendGoForwardJSONRequest(
          automation(), windex, tab_index, &error_msg)) {
    *error = new Error(kUnknownError, error_msg);
  }
}

void Automation::GoBack(int tab_id, Error** error) {
  int windex = 0, tab_index = 0;
  *error = GetIndicesForTab(tab_id, &windex, &tab_index);
  if (*error)
    return;

  std::string error_msg;
  if (!SendGoBackJSONRequest(automation(), windex, tab_index, &error_msg))
    *error = new Error(kUnknownError, error_msg);
}

void Automation::Reload(int tab_id, Error** error) {
  int windex = 0, tab_index = 0;
  *error = GetIndicesForTab(tab_id, &windex, &tab_index);
  if (*error)
    return;

  std::string error_msg;
  if (!SendReloadJSONRequest(automation(), windex, tab_index, &error_msg))
    *error = new Error(kUnknownError, error_msg);
}

void Automation::GetCookies(const std::string& url,
                            ListValue** cookies,
                            Error** error) {
  std::string error_msg;
  if (!SendGetCookiesJSONRequest(automation(), url, cookies, &error_msg))
    *error = new Error(kUnknownError, error_msg);
}

void Automation::DeleteCookie(const std::string& url,
                              const std::string& cookie_name,
                              Error** error) {
  std::string error_msg;
  if (!SendDeleteCookieJSONRequest(
          automation(), url, cookie_name, &error_msg)) {
    *error = new Error(kUnknownError, error_msg);
  }
}

void Automation::SetCookie(const std::string& url,
                           DictionaryValue* cookie_dict,
                           Error** error) {
  std::string error_msg;
  if (!SendSetCookieJSONRequest(automation(), url, cookie_dict, &error_msg))
    *error = new Error(kUnknownError, error_msg);
}

void Automation::GetTabIds(std::vector<int>* tab_ids,
                           Error** error) {
  std::string error_msg;
  if (!SendGetTabIdsJSONRequest(automation(), tab_ids, &error_msg))
    *error = new Error(kUnknownError, error_msg);
}

void Automation::DoesTabExist(int tab_id, bool* does_exist, Error** error) {
  std::string error_msg;
  if (!SendIsTabIdValidJSONRequest(
          automation(), tab_id, does_exist, &error_msg)) {
    *error = new Error(kUnknownError, error_msg);
  }
}

void Automation::CloseTab(int tab_id, Error** error) {
  int windex = 0, tab_index = 0;
  *error = GetIndicesForTab(tab_id, &windex, &tab_index);
  if (*error)
    return;

  std::string error_msg;
  if (!SendCloseTabJSONRequest(automation(), windex, tab_index, &error_msg))
    *error = new Error(kUnknownError, error_msg);
}

void Automation::GetAppModalDialogMessage(std::string* message, Error** error) {
  *error = CheckAlertsSupported();
  if (*error)
    return;

  std::string error_msg;
  if (!SendGetAppModalDialogMessageJSONRequest(
          automation(), message, &error_msg)) {
    *error = new Error(kUnknownError, error_msg);
  }
}

void Automation::AcceptOrDismissAppModalDialog(bool accept, Error** error) {
  *error = CheckAlertsSupported();
  if (*error)
    return;

  std::string error_msg;
  if (!SendAcceptOrDismissAppModalDialogJSONRequest(
          automation(), accept, &error_msg)) {
    *error = new Error(kUnknownError, error_msg);
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
    *error = new Error(kUnknownError, error_msg);
  }
}

void Automation::GetBrowserVersion(std::string* version) {
  *version = automation()->server_version();
}

void Automation::GetChromeDriverAutomationVersion(int* version, Error** error) {
  std::string error_msg;
  if (!SendGetChromeDriverAutomationVersion(automation(), version, &error_msg))
    *error = new Error(kUnknownError, error_msg);
}

void Automation::WaitForAllTabsToStopLoading(Error** error) {
  std::string error_msg;
  if (!SendWaitForAllTabsToStopLoadingJSONRequest(automation(), &error_msg))
    *error = new Error(kUnknownError, error_msg);
}

void Automation::InstallExtensionDeprecated(
    const FilePath& path, Error** error) {
  if (!launcher_->automation()->InstallExtension(path, false).get())
    *error = new Error(kUnknownError, "Failed to install extension");
}

void Automation::GetInstalledExtensions(
    std::vector<std::string>* extension_ids, Error** error) {
  *error = CheckNewExtensionInterfaceSupported();
  if (*error)
    return;

  std::string error_msg;
  base::ListValue extensions_list;
  if (!SendGetExtensionsInfoJSONRequest(
          automation(), &extensions_list, &error_msg)) {
    *error = new Error(kUnknownError, error_msg);
    return;
  }

  for (size_t i = 0; i < extensions_list.GetSize(); i++) {
    DictionaryValue* extension_dict;
    if (!extensions_list.GetDictionary(i, &extension_dict)) {
      *error = new Error(kUnknownError, "Invalid extension dictionary");
      return;
    }
    bool is_component;
    if (!extension_dict->GetBoolean("is_component", &is_component)) {
      *error = new Error(kUnknownError,
                         "Missing or invalid 'is_component_extension'");
      return;
    }
    if (is_component)
      continue;

    std::string extension_id;
    if (!extension_dict->GetString("id", &extension_id)) {
      *error = new Error(kUnknownError, "Missing or invalid 'id'");
      return;
    }
    extension_ids->push_back(extension_id);
  }
}

void Automation::InstallExtension(
    const FilePath& path, std::string* extension_id, Error** error) {
  *error = CheckNewExtensionInterfaceSupported();
  if (*error)
    return;

  std::string error_msg;
  if (!SendInstallExtensionJSONRequest(
          automation(), path, false /* with_ui */, extension_id, &error_msg))
    *error = new Error(kUnknownError, error_msg);
}

AutomationProxy* Automation::automation() const {
  return launcher_->automation();
}

Error* Automation::GetIndicesForTab(
    int tab_id, int* browser_index, int* tab_index) {
  std::string error_msg;
  if (!SendGetIndicesFromTabIdJSONRequest(
          automation(), tab_id, browser_index, tab_index, &error_msg)) {
    return new Error(kUnknownError, error_msg);
  }
  return NULL;
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

Error* Automation::CheckNewExtensionInterfaceSupported() {
  const char* message =
      "Extension interface is not supported for this version of Chrome";
  return CheckVersion(947, 0, message);
}

}  // namespace webdriver
