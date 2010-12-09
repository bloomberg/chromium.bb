// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CEEE module-wide utilities.

#include "ceee/ie/common/ceee_module_util.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/stringize_macros.h"
#include "base/win/registry.h"
#include "ceee/common/process_utils_win.h"
#include "chrome/installer/util/google_update_constants.h"

#include "version.h"  // NOLINT

namespace {

const wchar_t* kRegistryPath = L"SOFTWARE\\Google\\CEEE";
const wchar_t* kRegistryValueToolbandIsHidden = L"toolband_is_hidden";
const wchar_t* kRegistryValueToolbandPlaced = L"toolband_placed";
const wchar_t* kRegistryValueCrxInstalledPath = L"crx_installed_path";
const wchar_t* kRegistryValueCrxInstalledTime = L"crx_installed_time";
const wchar_t* kRegistryValueCrxInstalledByVersion =
    L"crx_installed_runtime_version";
const wchar_t* kRegistryValueEnableWebProgressApis =
    L"enable_webnav_webreq_apis";

// Global state needed by the BHO and the
// toolband, to indicate whether ShowDW calls should affect
// registry tracking of the user's visibility preference. A
// non-zero value indicates that the calls should be ignored.
LONG g_ignore_show_dw_changes = 0;

bool GetCeeeRegistryBoolean(const wchar_t* key, bool default_value) {
  base::win::RegKey hkcu(HKEY_CURRENT_USER, kRegistryPath, KEY_READ);
  LOG_IF(ERROR, !hkcu.Valid()) << "Could not open reg key: " << kRegistryPath;

  DWORD dword_value_representation = 0;
  DWORD size = sizeof(dword_value_representation);
  DWORD type = REG_DWORD;

  if (!hkcu.Valid() ||
      !hkcu.ReadValue(key, &dword_value_representation, &size, &type)) {
    return default_value;
  }

  return dword_value_representation != 0;
}

void SetCeeeRegistryBoolean(const wchar_t* key, bool assign_value) {
  base::win::RegKey hkcu(HKEY_CURRENT_USER, kRegistryPath, KEY_WRITE);
  LOG_IF(ERROR, !hkcu.Valid()) << "Could not open reg key: " << kRegistryPath;

  DWORD dword_value_representation = assign_value ? 1 : 0;
  bool write_result = hkcu.WriteValue(key, &dword_value_representation,
                                      sizeof(dword_value_representation),
                                      REG_DWORD);

  LOG_IF(ERROR, !write_result) << "Failed to write a registry key: " << key;
}

}  // anonymous namespace

namespace ceee_module_util {


// The name of the profile we want ChromeFrame to use (for Internet Explorer).
const wchar_t kChromeProfileName[] = L"iexplore";

// The name of the profile we want ChromeFrame to use (for Internet Explorer)
// in case when explorer is 'Run as Administrator'.
const wchar_t kChromeProfileNameForAdmin[] = L"iexplore_admin";

const wchar_t kChromeProfileNameForFirefox[] = L"ceee_ff";
const wchar_t kInternetExplorerModuleName[] = L"iexplore.exe";
const wchar_t kCeeeBrokerModuleName[] = L"ceee_broker.exe";

std::wstring GetExtensionPath() {
  const wchar_t* kRegistryValue = L"crx_path";

  // We first check HKEY_CURRENT_USER, then HKEY_LOCAL_MACHINE.  This is to
  // let individual users override the machine-wide setting.  The machine-wide
  // setting is needed as our installer runs as system.
  //
  // On 64-bit Windows, there are separate versions of the registry for 32-bit
  // applications and 64-bit applications.  When you manually set things in the
  // registry, you may have set them using the 32-bit regedit program
  // (generally at c:\windows\SysWOW64\regedit.exe) or the 64-bit version,
  // confusingly named regedt32.exe (at c:\windows\system32\regedt32.exe).
  //
  // You need to make sure you set the version of the registry corresponding to
  // the way this executable is compiled.
  //
  // If the registry values are not set, then attempt to locate the extension
  // by convention: that is, in a folder called
  // "[32-bit program files]\Google\CEEE\Extensions", look for the
  // first valid directory and use that.  Eventually, when we support more
  // than one extension, we can load/install all directories and/or crx files
  // found here.
  std::wstring crx_path;
  base::win::RegKey hkcu(HKEY_CURRENT_USER, kRegistryPath, KEY_READ);
  base::win::RegKey hklm(HKEY_LOCAL_MACHINE, kRegistryPath, KEY_READ);

  base::win::RegKey* keys[] = { &hkcu, &hklm };
  for (int i = 0; i < arraysize(keys); ++i) {
    if (keys[i]->Valid() && keys[i]->ReadValue(kRegistryValue, &crx_path))
      break;
  }

  if (crx_path.size() == 0u) {
    FilePath file_path;
    PathService::Get(base::DIR_PROGRAM_FILES, &file_path);

    file_path = file_path.Append(L"Google").Append(L"CEEE").
        Append(L"Extensions");
    if (!file_path.empty()) {
      // First check for a .crx file (we prefer the .crx)
      file_util::FileEnumerator e(
          file_path, false, file_util::FileEnumerator::FILES);
      for (FilePath ext = e.Next(); !ext.empty(); ext = e.Next()) {
        if (ext.Extension() == L".crx") {
          crx_path = ext.value();
          break;
        }
      }

      if (crx_path.size() == 0u) {
        // Use the first directory instead, in the hope that it is an
        // exploded extension directory.
        file_util::FileEnumerator e(
            file_path, false, file_util::FileEnumerator::DIRECTORIES);
        FilePath directory = e.Next();
        if (!directory.empty()) {
          crx_path = directory.value();
        }
      }
    }
  }

  // Don't DCHECK here - it's an expected case that no .crx file is provided
  // for installation along with CEEE.
  LOG_IF(WARNING, crx_path.empty()) << "No CRX found to install.";
  return crx_path;
}

FilePath GetInstalledExtensionPath() {
  std::wstring crx_path;

  base::win::RegKey hkcu(HKEY_CURRENT_USER, kRegistryPath, KEY_READ);
  // Default value is good enough for us.
  hkcu.ReadValue(kRegistryValueCrxInstalledPath, &crx_path);

  return FilePath(crx_path);
}

base::Time GetInstalledExtensionTime() {
  int64 crx_time = 0;
  DWORD size = sizeof(crx_time);

  base::win::RegKey hkcu(HKEY_CURRENT_USER, kRegistryPath, KEY_READ);
  // Default value is good enough for us.
  hkcu.ReadValue(kRegistryValueCrxInstalledTime, &crx_time, &size, NULL);

  return base::Time::FromInternalValue(crx_time);
}

bool NeedToInstallExtension() {
  std::wstring path = GetExtensionPath();
  if (path.empty()) {
    // No extension to install, so no need to install the extension.
    return false;
  }

  const FilePath crx_path(path);
  if (IsCrxOrEmpty(crx_path.value())) {
    if (crx_path == GetInstalledExtensionPath()) {
      base::Time installed_time;
      base::PlatformFileInfo extension_info;
      bool success = file_util::GetFileInfo(crx_path, &extension_info);
      // If the call above didn't succeed, assume we need to install.
      if (!success ||
          extension_info.last_modified > GetInstalledExtensionTime()) {
        return true;
      } else {
        // We also check that the current version of Chrome was the one
        // that attempted installation; if not, changes such as a change
        // in the location of a profile directory might have occurred, and
        // we might need to retry installation.
        std::wstring version_string;
        base::win::RegKey hkcu(HKEY_CURRENT_USER, kRegistryPath, KEY_READ);
        success = hkcu.ReadValue(
            kRegistryValueCrxInstalledByVersion, &version_string);
        return !success || version_string != TO_L_STRING(CHROME_VERSION_STRING);
      }
    }

    return true;
  }

  return false;
}

void SetInstalledExtensionPath(const FilePath& path) {
  base::PlatformFileInfo extension_info;
  const bool success = file_util::GetFileInfo(path, &extension_info);
  const int64 crx_time = success ?
      extension_info.last_modified.ToInternalValue() :
      base::Time::Now().ToInternalValue();

  base::win::RegKey key(HKEY_CURRENT_USER, kRegistryPath, KEY_WRITE);
  bool write_result = key.WriteValue(kRegistryValueCrxInstalledTime,
                                     &crx_time,
                                     sizeof(crx_time),
                                     REG_QWORD);
  DCHECK(write_result);
  write_result = key.WriteValue(kRegistryValueCrxInstalledPath,
                                path.value().c_str());
  DCHECK(write_result);

  write_result = key.WriteValue(kRegistryValueCrxInstalledByVersion,
                                TO_L_STRING(CHROME_VERSION_STRING));
}

bool IsCrxOrEmpty(const std::wstring& path) {
  return (path.empty() ||
      (path.substr(std::max(path.size() - 4, 0u)) == L".crx"));
}

void SetOptionToolbandIsHidden(bool is_hidden) {
  SetCeeeRegistryBoolean(kRegistryValueToolbandIsHidden, is_hidden);
}

bool GetOptionToolbandIsHidden() {
  return GetCeeeRegistryBoolean(kRegistryValueToolbandIsHidden, false);
}

void SetOptionToolbandForceReposition(bool reposition_next_time) {
  SetCeeeRegistryBoolean(kRegistryValueToolbandPlaced, !reposition_next_time);
}

bool GetOptionToolbandForceReposition() {
  return !GetCeeeRegistryBoolean(kRegistryValueToolbandPlaced, false);
}

void SetOptionEnableWebProgressApis(bool enable) {
  SetCeeeRegistryBoolean(kRegistryValueEnableWebProgressApis, enable);
}

bool GetOptionEnableWebProgressApis() {
  return GetCeeeRegistryBoolean(kRegistryValueEnableWebProgressApis, false);
}

void SetIgnoreShowDWChanges(bool ignore) {
  if (ignore) {
    ::InterlockedIncrement(&g_ignore_show_dw_changes);
  } else {
    ::InterlockedDecrement(&g_ignore_show_dw_changes);
  }
}

bool GetIgnoreShowDWChanges() {
  return ::InterlockedExchangeAdd(&g_ignore_show_dw_changes, 0) != 0;
}

const wchar_t* GetBrokerProfileNameForIe() {
  bool running_as_admin = false;
  HRESULT hr =
      process_utils_win::IsCurrentProcessUacElevated(&running_as_admin);
  DCHECK(SUCCEEDED(hr));

  // Profile name for 'runas' mode has to be different because this mode spawns
  // its own copy of ceee_broker. To avoid scrambling user data, we run it in
  // different profile. In the unlikely event of even failing to retrieve info,
  // run as normal user.
  return (FAILED(hr) || !running_as_admin) ? kChromeProfileName
                                           : kChromeProfileNameForAdmin;
}

// TODO(vitalybuka@google.com) : remove this code and use BrowserDistribution.
// BrowserDistribution requires modification to know about CEEE (bb3136374).
std::wstring GetCromeFrameClientStateKey() {
  static const wchar_t kChromeFrameGuid[] =
      L"{8BA986DA-5100-405E-AA35-86F34A02ACBF}";
  std::wstring key(google_update::kRegPathClientState);
  key.append(L"\\");
  key.append(kChromeFrameGuid);
  return key;
}

// TODO(vitalybuka@google.com) : remove this code and use
// GoogleUpdateSettings::GetCollectStatsConsent() code.
// BrowserDistribution requires modification to know about CEEE (bb3136374).
bool GetCollectStatsConsent() {
  std::wstring reg_path = GetCromeFrameClientStateKey();
  base::win::RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ);
  DWORD value;
  if (!key.ReadValueDW(google_update::kRegUsageStatsField, &value)) {
    base::win::RegKey hklm_key(HKEY_LOCAL_MACHINE, reg_path.c_str(), KEY_READ);
    if (!hklm_key.ReadValueDW(google_update::kRegUsageStatsField, &value))
      return false;
  }
  return (1 == value);
}

}  // namespace ceee_module_util
