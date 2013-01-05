// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include <windows.h>
#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/scoped_comptr.h"
#include "base/win/shortcut.h"
#include "base/win/windows_version.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/create_reg_key_work_item.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/set_reg_value_work_item.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

#if defined(GOOGLE_CHROME_BUILD)
const wchar_t kAppListAppName[] = L"ChromeAppList";
#else
const wchar_t kAppListAppName[] = L"ChromiumAppList";
#endif

// Helper function for ShellIntegration::GetAppId to generates profile id
// from profile path. "profile_id" is composed of sanitized basenames of
// user data dir and profile dir joined by a ".".
string16 GetProfileIdFromPath(const FilePath& profile_path) {
  // Return empty string if profile_path is empty
  if (profile_path.empty())
    return string16();

  FilePath default_user_data_dir;
  // Return empty string if profile_path is in default user data
  // dir and is the default profile.
  if (chrome::GetDefaultUserDataDirectory(&default_user_data_dir) &&
      profile_path.DirName() == default_user_data_dir &&
      profile_path.BaseName().value() ==
          ASCIIToUTF16(chrome::kInitialProfile)) {
    return string16();
  }

  // Get joined basenames of user data dir and profile.
  string16 basenames = profile_path.DirName().BaseName().value() +
      L"." + profile_path.BaseName().value();

  string16 profile_id;
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

bool GetShortcutAppId(IShellLink* shell_link, string16* app_id) {
  DCHECK(shell_link);
  DCHECK(app_id);

  app_id->clear();

  base::win::ScopedComPtr<IPropertyStore> property_store;
  if (FAILED(property_store.QueryFrom(shell_link)))
    return false;

  PROPVARIANT appid_value;
  PropVariantInit(&appid_value);
  if (FAILED(property_store->GetValue(PKEY_AppUserModel_ID, &appid_value)))
    return false;

  if (appid_value.vt == VT_LPWSTR || appid_value.vt == VT_BSTR)
    app_id->assign(appid_value.pwszVal);

  PropVariantClear(&appid_value);
  return true;
}

// Gets expected app id for given chrome shortcut. Returns true if the shortcut
// points to chrome and expected app id is successfully derived.
bool GetExpectedAppId(const FilePath& chrome_exe,
                      IShellLink* shell_link,
                      string16* expected_app_id) {
  DCHECK(shell_link);
  DCHECK(expected_app_id);

  expected_app_id->clear();

  // Check if the shortcut points to chrome_exe.
  string16 source;
  if (FAILED(shell_link->GetPath(WriteInto(&source, MAX_PATH), MAX_PATH, NULL,
                                 SLGP_RAWPATH)) ||
      lstrcmpi(chrome_exe.value().c_str(), source.c_str()))
    return false;

  string16 arguments;
  if (FAILED(shell_link->GetArguments(WriteInto(&arguments, MAX_PATH),
                                      MAX_PATH)))
    return false;

  // Get expected app id from shortcut command line.
  CommandLine command_line = CommandLine::FromString(base::StringPrintf(
      L"\"%ls\" %ls", source.c_str(), arguments.c_str()));

  FilePath profile_path;
  if (command_line.HasSwitch(switches::kUserDataDir)) {
    profile_path =
        command_line.GetSwitchValuePath(switches::kUserDataDir).AppendASCII(
            chrome::kInitialProfile);
  }

  string16 app_name;
  if (command_line.HasSwitch(switches::kApp)) {
    app_name = UTF8ToUTF16(web_app::GenerateApplicationNameFromURL(
        GURL(command_line.GetSwitchValueASCII(switches::kApp))));
  } else if (command_line.HasSwitch(switches::kAppId)) {
    app_name = UTF8ToUTF16(web_app::GenerateApplicationNameFromExtensionId(
        command_line.GetSwitchValueASCII(switches::kAppId)));
  } else if (command_line.HasSwitch(switches::kShowAppList)) {
    app_name = kAppListAppName;
  } else {
    BrowserDistribution* dist = BrowserDistribution::GetDistribution();
    app_name = ShellUtil::GetBrowserModelId(
        dist, InstallUtil::IsPerUserInstall(chrome_exe.value().c_str()));
  }

  expected_app_id->assign(
      ShellIntegration::GetAppModelIdForProfile(app_name, profile_path));
  return true;
}

void MigrateWin7ShortcutsInPath(
    const FilePath& chrome_exe, const FilePath& path) {
  // Enumerate all pinned shortcuts in the given path directly.
  file_util::FileEnumerator shortcuts_enum(
      path, false,  // not recursive
      file_util::FileEnumerator::FILES, FILE_PATH_LITERAL("*.lnk"));

  for (FilePath shortcut = shortcuts_enum.Next(); !shortcut.empty();
       shortcut = shortcuts_enum.Next()) {
    // Load the shortcut.
    base::win::ScopedComPtr<IShellLink> shell_link;
    if (FAILED(shell_link.CreateInstance(CLSID_ShellLink, NULL,
                                         CLSCTX_INPROC_SERVER))) {
      NOTREACHED();
      return;
    }

    base::win::ScopedComPtr<IPersistFile> persist_file;
    if (FAILED(persist_file.QueryFrom(shell_link)) ||
        FAILED(persist_file->Load(shortcut.value().c_str(), STGM_READ))) {
      NOTREACHED();
      return;
    }

    // Get expected app id from shortcut.
    string16 expected_app_id;
    if (!GetExpectedAppId(chrome_exe, shell_link, &expected_app_id) ||
        expected_app_id.empty())
      continue;

    // Get existing app id from shortcut if any.
    string16 existing_app_id;
    GetShortcutAppId(shell_link, &existing_app_id);

    if (expected_app_id != existing_app_id) {
      base::win::ShortcutProperties properties_app_id_only;
      properties_app_id_only.set_app_id(expected_app_id);
      base::win::CreateOrUpdateShortcutLink(
          shortcut, properties_app_id_only,
          base::win::SHORTCUT_UPDATE_EXISTING);
    }
  }
}

void MigrateChromiumShortcutsCallback() {
  // This should run on the file thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Get full path of chrome.
  FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe))
    return;

  // Locations to check for shortcuts migration.
  static const struct {
    int location_id;
    const wchar_t* sub_dir;
  } kLocations[] = {
    {
      base::DIR_TASKBAR_PINS,
      NULL
    }, {
      base::DIR_USER_DESKTOP,
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

    MigrateWin7ShortcutsInPath(chrome_exe, path);
  }
}

ShellIntegration::DefaultWebClientState
    GetDefaultWebClientStateFromShellUtilDefaultState(
        ShellUtil::DefaultState default_state) {
  switch (default_state) {
    case ShellUtil::NOT_DEFAULT:
      return ShellIntegration::NOT_DEFAULT;
    case ShellUtil::IS_DEFAULT:
      return ShellIntegration::IS_DEFAULT;
    default:
      DCHECK_EQ(ShellUtil::UNKNOWN_DEFAULT, default_state);
      return ShellIntegration::UNKNOWN_DEFAULT;
  }
}

}  // namespace

ShellIntegration::DefaultWebClientSetPermission
    ShellIntegration::CanSetAsDefaultBrowser() {
  if (!BrowserDistribution::GetDistribution()->CanSetAsDefault())
    return SET_DEFAULT_NOT_ALLOWED;

  if (ShellUtil::CanMakeChromeDefaultUnattended())
    return SET_DEFAULT_UNATTENDED;
  else
    return SET_DEFAULT_INTERACTIVE;
}

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

bool ShellIntegration::SetAsDefaultProtocolClient(const std::string& protocol) {
  if (protocol.empty())
    return false;

  FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    LOG(ERROR) << "Error getting app exe path";
    return false;
  }

  string16 wprotocol(UTF8ToUTF16(protocol));
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (!ShellUtil::MakeChromeDefaultProtocolClient(dist, chrome_exe.value(),
        wprotocol)) {
    LOG(ERROR) << "Chrome could not be set as default handler for "
               << protocol << ".";
    return false;
  }

  VLOG(1) << "Chrome registered as default handler for " << protocol << ".";
  return true;
}

bool ShellIntegration::SetAsDefaultBrowserInteractive() {
  FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED() << "Error getting app exe path";
    return false;
  }

  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (!ShellUtil::ShowMakeChromeDefaultSystemUI(dist, chrome_exe.value())) {
    LOG(ERROR) << "Failed to launch the set-default-browser Windows UI.";
    return false;
  }

  VLOG(1) << "Set-default-browser Windows UI completed.";
  return true;
}

bool ShellIntegration::SetAsDefaultProtocolClientInteractive(
    const std::string& protocol) {
  FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED() << "Error getting app exe path";
    return false;
  }

  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  string16 wprotocol(UTF8ToUTF16(protocol));
  if (!ShellUtil::ShowMakeChromeDefaultProtocolClientSystemUI(
          dist, chrome_exe.value(), wprotocol)) {
    LOG(ERROR) << "Failed to launch the set-default-client Windows UI.";
    return false;
  }

  VLOG(1) << "Set-default-client Windows UI completed.";
  return true;
}

ShellIntegration::DefaultWebClientState ShellIntegration::GetDefaultBrowser() {
  return GetDefaultWebClientStateFromShellUtilDefaultState(
      ShellUtil::GetChromeDefaultState());
}

ShellIntegration::DefaultWebClientState
    ShellIntegration::IsDefaultProtocolClient(const std::string& protocol) {
  return GetDefaultWebClientStateFromShellUtilDefaultState(
      ShellUtil::GetChromeDefaultProtocolClientState(UTF8ToUTF16(protocol)));
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
    string16 app_cmd;
    base::win::RegKey key(HKEY_CURRENT_USER,
                          ShellUtil::kRegVistaUrlPrefs, KEY_READ);
    if (key.Valid() && (key.ReadValue(L"Progid", &app_cmd) == ERROR_SUCCESS) &&
        app_cmd == L"FirefoxURL")
      ff_default = true;
  } else {
    string16 key_path(L"http");
    key_path.append(ShellUtil::kRegShellOpen);
    base::win::RegKey key(HKEY_CLASSES_ROOT, key_path.c_str(), KEY_READ);
    string16 app_cmd;
    if (key.Valid() && (key.ReadValue(L"", &app_cmd) == ERROR_SUCCESS) &&
        string16::npos != StringToLowerASCII(app_cmd).find(L"firefox"))
      ff_default = true;
  }
  return ff_default;
}

string16 ShellIntegration::GetAppModelIdForProfile(
    const string16& app_name,
    const FilePath& profile_path) {
  std::vector<string16> components;
  components.push_back(app_name);
  const string16 profile_id(GetProfileIdFromPath(profile_path));
  if (!profile_id.empty())
    components.push_back(profile_id);
  return ShellUtil::BuildAppModelId(components);
}

string16 ShellIntegration::GetChromiumModelIdForProfile(
    const FilePath& profile_path) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
    return dist->GetBaseAppId();
  }
  return GetAppModelIdForProfile(
      ShellUtil::GetBrowserModelId(
           dist, InstallUtil::IsPerUserInstall(chrome_exe.value().c_str())),
      profile_path);
}

string16 ShellIntegration::GetAppListAppModelIdForProfile(
    const FilePath& profile_path) {
  return ShellIntegration::GetAppModelIdForProfile(kAppListAppName,
                                                   profile_path);
}

string16 ShellIntegration::GetChromiumIconLocation() {
  // Determine the path to chrome.exe. If we can't determine what that is,
  // we have bigger fish to fry...
  FilePath chrome_exe;
  if (!PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
    return string16();
  }

  return ShellUtil::FormatIconLocation(
      chrome_exe.value(),
      BrowserDistribution::GetDistribution()->GetIconIndex());
}

void ShellIntegration::MigrateChromiumShortcuts() {
  if (base::win::GetVersion() < base::win::VERSION_WIN7)
    return;

  // This needs to happen eventually (e.g. so that the appid is fixed and the
  // run-time Chrome icon is merged with the taskbar shortcut), but this is not
  // urgent and shouldn't delay Chrome startup.
  static const int64 kMigrateChromiumShortcutsDelaySeconds = 15;
  BrowserThread::PostDelayedTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&MigrateChromiumShortcutsCallback),
      base::TimeDelta::FromSeconds(kMigrateChromiumShortcutsDelaySeconds));
}

FilePath ShellIntegration::GetStartMenuShortcut(const FilePath& chrome_exe) {
  static const int kFolderIds[] = {
    base::DIR_COMMON_START_MENU,
    base::DIR_START_MENU,
  };
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  string16 shortcut_name(dist->GetAppShortCutName());
  FilePath shortcut;

  // Check both the common and the per-user Start Menu folders for system-level
  // installs.
  size_t folder =
      InstallUtil::IsPerUserInstall(chrome_exe.value().c_str()) ? 1 : 0;
  for (; folder < arraysize(kFolderIds); ++folder) {
    if (!PathService::Get(kFolderIds[folder], &shortcut)) {
      NOTREACHED();
      continue;
    }

    shortcut = shortcut.Append(shortcut_name).Append(shortcut_name +
                                                     installer::kLnkExt);
    if (file_util::PathExists(shortcut))
      return shortcut;
  }

  return FilePath();
}
