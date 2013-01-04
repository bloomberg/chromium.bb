// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include <windows.h>
#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>
#include <tchar.h>
#include <strsafe.h>

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

// propsys.lib is required for PropvariantTo*().
#pragma comment(lib, "propsys.lib")

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

// Gets expected app id for given Chrome (based on |command_line| and
// |is_per_user_install|).
string16 GetExpectedAppId(const CommandLine& command_line,
                          bool is_per_user_install) {
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
    app_name = ShellUtil::GetBrowserModelId(dist, is_per_user_install);
  }

  return ShellIntegration::GetAppModelIdForProfile(app_name, profile_path);
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

    bool check_dual_mode = (kLocations[i].location_id == base::DIR_START_MENU);
    ShellIntegration::MigrateShortcutsInPathInternal(chrome_exe, path,
                                                     check_dual_mode);
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

int ShellIntegration::MigrateShortcutsInPathInternal(const FilePath& chrome_exe,
                                                     const FilePath& path,
                                                     bool check_dual_mode) {
  DCHECK(base::win::GetVersion() >= base::win::VERSION_WIN7);

  // Enumerate all pinned shortcuts in the given path directly.
  file_util::FileEnumerator shortcuts_enum(
      path, false,  // not recursive
      file_util::FileEnumerator::FILES, FILE_PATH_LITERAL("*.lnk"));

  bool is_per_user_install =
      InstallUtil::IsPerUserInstall(chrome_exe.value().c_str());

  int shortcuts_migrated = 0;
  FilePath target_path;
  string16 arguments;
  string16 existing_app_id;
  for (FilePath shortcut = shortcuts_enum.Next(); !shortcut.empty();
       shortcut = shortcuts_enum.Next()) {
    // TODO(gab): Use ProgramCompare instead of comparing FilePaths below once
    // it is fixed to work with FilePaths with spaces.
    if (!base::win::ResolveShortcut(shortcut, &target_path, &arguments) ||
        chrome_exe != target_path) {
      continue;
    }
    CommandLine command_line(CommandLine::FromString(base::StringPrintf(
        L"\"%ls\" %ls", target_path.value().c_str(), arguments.c_str())));

    // Get the expected AppId for this Chrome shortcut.
    string16 expected_app_id(
        GetExpectedAppId(command_line, is_per_user_install));
    if (expected_app_id.empty())
      continue;

    // Load the shortcut.
    base::win::ScopedComPtr<IShellLink> shell_link;
    base::win::ScopedComPtr<IPersistFile> persist_file;
    if (FAILED(shell_link.CreateInstance(CLSID_ShellLink, NULL,
                                         CLSCTX_INPROC_SERVER)) ||
        FAILED(persist_file.QueryFrom(shell_link)) ||
        FAILED(persist_file->Load(shortcut.value().c_str(), STGM_READ))) {
      DLOG(WARNING) << "Failed loading shortcut at " << shortcut.value();
      continue;
    }

    // Any properties that need to be updated on the shortcut will be stored in
    // |updated_properties|.
    base::win::ShortcutProperties updated_properties;

    // Validate the existing app id for the shortcut.
    base::win::ScopedComPtr<IPropertyStore> property_store;
    PROPVARIANT pv_app_id;
    PropVariantInit(&pv_app_id);
    if (FAILED(property_store.QueryFrom(shell_link)) ||
        property_store->GetValue(PKEY_AppUserModel_ID, &pv_app_id) != S_OK) {
      // When in doubt, prefer not updating the shortcut.
      NOTREACHED();
      continue;
    } else if (pv_app_id.vt == VT_EMPTY) {
      // If there is no app_id set, set our app_id if one is expected.
      if (!expected_app_id.empty())
        updated_properties.set_app_id(expected_app_id);
    } else {
      // Validate that the existing app_id is the expected app_id; if not, set
      // the expected app_id on the shortcut.
      size_t expected_size = expected_app_id.size() + 1;
      HRESULT result = PropVariantToString(
          pv_app_id, WriteInto(&existing_app_id, expected_size), expected_size);
      PropVariantClear(&pv_app_id);
      if (result != S_OK && result != STRSAFE_E_INSUFFICIENT_BUFFER) {
        // Accept the STRSAFE_E_INSUFFICIENT_BUFFER error state as it means the
        // existing appid is longer than |expected_app_id| and thus we will
        // simply assume inequality.
        NOTREACHED();
        continue;
      } else if (result == STRSAFE_E_INSUFFICIENT_BUFFER ||
                 expected_app_id != existing_app_id) {
        updated_properties.set_app_id(expected_app_id);
      }
    }

    if (check_dual_mode) {
      BOOL existing_dual_mode;
      PROPVARIANT pv_dual_mode;
      PropVariantInit(&pv_dual_mode);
      if (property_store->GetValue(PKEY_AppUserModel_IsDualMode,
                                   &pv_dual_mode) != S_OK) {
        // When in doubt, prefer to not update the shortcut.
        NOTREACHED();
        continue;
      } else if (pv_dual_mode.vt == VT_EMPTY) {
        // If dual_mode is not set at all, make sure it gets set to true.
        updated_properties.set_dual_mode(true);
      } else {
        // If it is set to false, make sure it gets set to true as well.
        if (PropVariantToBoolean(pv_dual_mode, &existing_dual_mode) != S_OK) {
          NOTREACHED();
          continue;
        }
        PropVariantClear(&pv_dual_mode);
        if (!existing_dual_mode)
          updated_properties.set_dual_mode(true);
      }
    }

    persist_file.Release();
    shell_link.Release();

    // Update the shortcut if some of its properties need to be updated.
    if (updated_properties.options &&
        base::win::CreateOrUpdateShortcutLink(
            shortcut, updated_properties,
            base::win::SHORTCUT_UPDATE_EXISTING)) {
      ++shortcuts_migrated;
    }
  }
  return shortcuts_migrated;
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
