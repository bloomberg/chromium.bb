// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include <windows.h>
#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/scoped_comptr_win.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"
#include "content/browser/browser_thread.h"

namespace {

// Helper function for ShellIntegration::GetAppId to generates profile id
// from profile path. "profile_id" is composed of sanitized basenames of
// user data dir and profile dir joined by a ".".
std::wstring GetProfileIdFromPath(const FilePath& profile_path) {
  // Return empty string if profile_path is empty
  if (profile_path.empty())
    return std::wstring();

  FilePath default_user_data_dir;
  // Return empty string if profile_path is in default user data
  // dir and is the default profile.
  if (chrome::GetDefaultUserDataDirectory(&default_user_data_dir) &&
      profile_path.DirName() == default_user_data_dir &&
      profile_path.BaseName().value() == chrome::kNotSignedInProfile)
    return std::wstring();

  // Get joined basenames of user data dir and profile.
  std::wstring basenames = profile_path.DirName().BaseName().value() +
      L"." + profile_path.BaseName().value();

  std::wstring profile_id;
  profile_id.reserve(basenames.size());

  // Generate profile_id from sanitized basenames.
  for (size_t i = 0; i < basenames.length(); ++i) {
    if (IsAsciiAlpha(basenames[i]) ||
        IsAsciiDigit(basenames[i]) ||
        basenames[i] == L'.')
      profile_id += basenames[i];
  }

  return profile_id;
}

// Migrates existing chromium shortucts for Win7.
class MigrateChromiumShortcutsTask : public Task {
 public:
  MigrateChromiumShortcutsTask() { }

  virtual void Run();

 private:
  void MigrateWin7Shortcuts();
  void MigrateWin7ShortcutsInPath(const FilePath& path) const;

  // Get expected app id for given chrome shortcut.
  // Returns true if the shortcut point to chrome and expected app id is
  // successfully derived.
  bool GetExpectedAppId(IShellLink* shell_link,
                        std::wstring* expected_app_id) const;

  // Get app id associated with given shortcut.
  bool GetShortcutAppId(IShellLink* shell_link, std::wstring* app_id) const;

  FilePath chrome_exe_;

  DISALLOW_COPY_AND_ASSIGN(MigrateChromiumShortcutsTask);
};

void MigrateChromiumShortcutsTask::Run() {
  // This should run on the file thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  MigrateWin7Shortcuts();
}

void MigrateChromiumShortcutsTask::MigrateWin7Shortcuts() {
  // Get full path of chrome.
  if (!PathService::Get(base::FILE_EXE, &chrome_exe_))
    return;

  // Locations to check for shortcuts migration.
  static const struct {
    int location_id;
    const wchar_t* sub_dir;
  } kLocations[] = {
    {
      base::DIR_APP_DATA,
      L"Microsoft\\Internet Explorer\\Quick Launch\\User Pinned\\TaskBar"
    }, {
      chrome::DIR_USER_DESKTOP,
      NULL
    }, {
      base::DIR_START_MENU,
      NULL
    }, {
      base::DIR_APP_DATA,
      L"Microsoft\\Internet Explorer\\Quick Launch\\User Pinned\\StartMenu"
    }
  };

  for (int i = 0; i < arraysize(kLocations); ++i) {
    FilePath path;
    if (!PathService::Get(kLocations[i].location_id, &path)) {
      NOTREACHED();
      continue;
    }

    if (kLocations[i].sub_dir)
      path = path.Append(kLocations[i].sub_dir);

    MigrateWin7ShortcutsInPath(path);
  }
}

void MigrateChromiumShortcutsTask::MigrateWin7ShortcutsInPath(
    const FilePath& path) const {
  // Enumerate all pinned shortcuts in the given path directly.
  file_util::FileEnumerator shortcuts_enum(
      path,
      false,  // not recursive
      file_util::FileEnumerator::FILES,
      FILE_PATH_LITERAL("*.lnk"));

  for (FilePath shortcut = shortcuts_enum.Next(); !shortcut.empty();
       shortcut = shortcuts_enum.Next()) {
    // Load the shortcut.
    ScopedComPtr<IShellLink> shell_link;
    if (FAILED(shell_link.CreateInstance(CLSID_ShellLink,
                                         NULL,
                                         CLSCTX_INPROC_SERVER))) {
      NOTREACHED();
      return;
    }

    ScopedComPtr<IPersistFile> persist_file;
    if (FAILED(persist_file.QueryFrom(shell_link)) ||
        FAILED(persist_file->Load(shortcut.value().c_str(), STGM_READ))) {
      NOTREACHED();
      return;
    }

    // Get expected app id from shortcut.
    std::wstring expected_app_id;
    if (!GetExpectedAppId(shell_link, &expected_app_id) ||
        expected_app_id.empty())
      continue;

    // Get existing app id from shortcut if any.
    std::wstring existing_app_id;
    GetShortcutAppId(shell_link, &existing_app_id);

    if (expected_app_id != existing_app_id) {
      file_util::UpdateShortcutLink(NULL,
                                    shortcut.value().c_str(),
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL,
                                    0,
                                    expected_app_id.c_str());
    }
  }
}

bool MigrateChromiumShortcutsTask::GetExpectedAppId(
    IShellLink* shell_link,
    std::wstring* expected_app_id) const {
  DCHECK(shell_link);
  DCHECK(expected_app_id);

  expected_app_id->clear();

  // Check if the shortcut points to chrome_exe.
  std::wstring source;
  if (FAILED(shell_link->GetPath(WriteInto(&source, MAX_PATH),
                                 MAX_PATH,
                                 NULL,
                                 SLGP_RAWPATH)) ||
      lstrcmpi(chrome_exe_.value().c_str(), source.c_str()))
    return false;

  std::wstring arguments;
  if (FAILED(shell_link->GetArguments(WriteInto(&arguments, MAX_PATH),
                                      MAX_PATH)))
    return false;

  // Get expected app id from shortcut command line.
  CommandLine command_line = CommandLine::FromString(StringPrintf(
      L"\"%ls\" %ls", source.c_str(), arguments.c_str()));

  FilePath profile_path;
  if (command_line.HasSwitch(switches::kUserDataDir)) {
    profile_path =
        command_line.GetSwitchValuePath(switches::kUserDataDir).Append(
            chrome::kNotSignedInProfile);
  }

  std::wstring app_name;
  if (command_line.HasSwitch(switches::kApp)) {
    app_name = UTF8ToWide(web_app::GenerateApplicationNameFromURL(
        GURL(command_line.GetSwitchValueASCII(switches::kApp))));
  } else if (command_line.HasSwitch(switches::kAppId)) {
    app_name = UTF8ToWide(web_app::GenerateApplicationNameFromExtensionId(
        command_line.GetSwitchValueASCII(switches::kAppId)));
  } else {
    app_name = BrowserDistribution::GetDistribution()->GetBrowserAppId();
  }

  expected_app_id->assign(ShellIntegration::GetAppId(app_name, profile_path));
  return true;
}

bool MigrateChromiumShortcutsTask::GetShortcutAppId(
    IShellLink* shell_link,
    std::wstring* app_id) const {
  DCHECK(shell_link);
  DCHECK(app_id);

  app_id->clear();

  ScopedComPtr<IPropertyStore> property_store;
  if (FAILED(property_store.QueryFrom(shell_link)))
    return false;

  PROPVARIANT appid_value;
  PropVariantInit(&appid_value);
  if (FAILED(property_store->GetValue(PKEY_AppUserModel_ID,
                                      &appid_value)))
    return false;

  if (appid_value.vt == VT_LPWSTR ||
      appid_value.vt == VT_BSTR)
    app_id->assign(appid_value.pwszVal);

  PropVariantClear(&appid_value);
  return true;
}

};

bool ShellIntegration::SetAsDefaultBrowser() {
  FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    LOG(ERROR) << "Error getting app exe path";
    return false;
  }

  // From UI currently we only allow setting default browser for current user.
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (!ShellUtil::MakeChromeDefault(dist, ShellUtil::CURRENT_USER,
                                    chrome_exe.value(), true)) {
    LOG(ERROR) << "Chrome could not be set as default browser.";
    return false;
  }

  VLOG(1) << "Chrome registered as default browser.";
  return true;
}

ShellIntegration::DefaultBrowserState ShellIntegration::IsDefaultBrowser() {
  // First determine the app path. If we can't determine what that is, we have
  // bigger fish to fry...
  FilePath app_path;
  if (!PathService::Get(base::FILE_EXE, &app_path)) {
    LOG(ERROR) << "Error getting app exe path";
    return UNKNOWN_DEFAULT_BROWSER;
  }
  // When we check for default browser we don't necessarily want to count file
  // type handlers and icons as having changed the default browser status,
  // since the user may have changed their shell settings to cause HTML files
  // to open with a text editor for example. We also don't want to aggressively
  // claim FTP, since the user may have a separate FTP client. It is an open
  // question as to how to "heal" these settings. Perhaps the user should just
  // re-run the installer or run with the --set-default-browser command line
  // flag. There is doubtless some other key we can hook into to cause "Repair"
  // to show up in Add/Remove programs for us.
  const std::wstring kChromeProtocols[] = {L"http", L"https"};

  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    IApplicationAssociationRegistration* pAAR;
    HRESULT hr = CoCreateInstance(CLSID_ApplicationAssociationRegistration,
        NULL, CLSCTX_INPROC, __uuidof(IApplicationAssociationRegistration),
        (void**)&pAAR);
    if (!SUCCEEDED(hr))
      return NOT_DEFAULT_BROWSER;

    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    std::wstring app_name = dist->GetApplicationName();
    // If a user specific default browser entry exists, we check for that
    // app name being default. If not, then default browser is just called
    // Google Chrome or Chromium so we do not append suffix to app name.
    std::wstring suffix;
    if (ShellUtil::GetUserSpecificDefaultBrowserSuffix(dist, &suffix))
      app_name += suffix;

    for (int i = 0; i < _countof(kChromeProtocols); i++) {
      BOOL result = TRUE;
      hr = pAAR->QueryAppIsDefault(kChromeProtocols[i].c_str(), AT_URLPROTOCOL,
          AL_EFFECTIVE, app_name.c_str(), &result);
      if (!SUCCEEDED(hr) || result == FALSE) {
        pAAR->Release();
        return NOT_DEFAULT_BROWSER;
      }
    }
    pAAR->Release();
  } else {
    std::wstring short_app_path;
    GetShortPathName(app_path.value().c_str(),
                     WriteInto(&short_app_path, MAX_PATH),
                     MAX_PATH);

    // open command for protocol associations
    for (int i = 0; i < _countof(kChromeProtocols); i++) {
      // Check in HKEY_CLASSES_ROOT that is the result of merge between
      // HKLM and HKCU
      HKEY root_key = HKEY_CLASSES_ROOT;
      // Check <protocol>\shell\open\command
      std::wstring key_path(kChromeProtocols[i] + ShellUtil::kRegShellOpen);
      base::win::RegKey key(root_key, key_path.c_str(), KEY_READ);
      std::wstring value;
      if (!key.Valid() || (key.ReadValue(L"", &value) != ERROR_SUCCESS))
        return NOT_DEFAULT_BROWSER;
      // Need to normalize path in case it's been munged.
      CommandLine command_line = CommandLine::FromString(value);
      std::wstring short_path;
      GetShortPathName(command_line.GetProgram().value().c_str(),
                       WriteInto(&short_path, MAX_PATH), MAX_PATH);
      if (!FilePath::CompareEqualIgnoreCase(short_path, short_app_path))
        return NOT_DEFAULT_BROWSER;
    }
  }
  return IS_DEFAULT_BROWSER;
}

// There is no reliable way to say which browser is default on a machine (each
// browser can have some of the protocols/shortcuts). So we look for only HTTP
// protocol handler. Even this handler is located at different places in
// registry on XP and Vista:
// - HKCR\http\shell\open\command (XP)
// - HKCU\Software\Microsoft\Windows\Shell\Associations\UrlAssociations\
//   http\UserChoice (Vista)
// This method checks if Firefox is defualt browser by checking these
// locations and returns true if Firefox traces are found there. In case of
// error (or if Firefox is not found)it returns the default value which
// is false.
bool ShellIntegration::IsFirefoxDefaultBrowser() {
  bool ff_default = false;
  if (base::win::GetVersion() >= base::win::VERSION_VISTA) {
    std::wstring app_cmd;
    base::win::RegKey key(HKEY_CURRENT_USER,
                          ShellUtil::kRegVistaUrlPrefs, KEY_READ);
    if (key.Valid() && (key.ReadValue(L"Progid", &app_cmd) == ERROR_SUCCESS) &&
        app_cmd == L"FirefoxURL")
      ff_default = true;
  } else {
    std::wstring key_path(L"http");
    key_path.append(ShellUtil::kRegShellOpen);
    base::win::RegKey key(HKEY_CLASSES_ROOT, key_path.c_str(), KEY_READ);
    std::wstring app_cmd;
    if (key.Valid() && (key.ReadValue(L"", &app_cmd) == ERROR_SUCCESS) &&
        std::wstring::npos != StringToLowerASCII(app_cmd).find(L"firefox"))
      ff_default = true;
  }
  return ff_default;
}

std::wstring ShellIntegration::GetAppId(const std::wstring& app_name,
                                        const FilePath& profile_path) {
  std::wstring app_id(app_name);

  std::wstring profile_id(GetProfileIdFromPath(profile_path));
  if (!profile_id.empty()) {
    app_id += L".";
    app_id += profile_id;
  }

  // App id should be less than 128 chars.
  DCHECK(app_id.length() < 128);
  return app_id;
}

std::wstring ShellIntegration::GetChromiumAppId(const FilePath& profile_path) {
  return GetAppId(BrowserDistribution::GetDistribution()->GetBrowserAppId(),
                  profile_path);
}

void ShellIntegration::MigrateChromiumShortcuts() {
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE, new MigrateChromiumShortcutsTask());
}
