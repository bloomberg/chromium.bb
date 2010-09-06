// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/google_update_settings.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/registry.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/install_util.h"

namespace {

bool ReadGoogleUpdateStrKey(const wchar_t* const name, std::wstring* value) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring reg_path = dist->GetStateKey();
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ);
  if (!key.ReadValue(name, value)) {
    RegKey hklm_key(HKEY_LOCAL_MACHINE, reg_path.c_str(), KEY_READ);
    return hklm_key.ReadValue(name, value);
  }
  return true;
}

bool WriteGoogleUpdateStrKey(const wchar_t* const name,
                             const std::wstring& value) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring reg_path = dist->GetStateKey();
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ | KEY_WRITE);
  return key.WriteValue(name, value.c_str());
}

bool ClearGoogleUpdateStrKey(const wchar_t* const name) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring reg_path = dist->GetStateKey();
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ | KEY_WRITE);
  std::wstring value;
  if (!key.ReadValue(name, &value))
    return false;
  return key.WriteValue(name, L"");
}

bool RemoveGoogleUpdateStrKey(const wchar_t* const name) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring reg_path = dist->GetStateKey();
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ | KEY_WRITE);
  if (!key.ValueExists(name))
    return true;
  return key.DeleteValue(name);
}

}  // namespace.

bool GoogleUpdateSettings::GetCollectStatsConsent() {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring reg_path = dist->GetStateKey();
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ);
  DWORD value;
  if (!key.ReadValueDW(google_update::kRegUsageStatsField, &value)) {
    RegKey hklm_key(HKEY_LOCAL_MACHINE, reg_path.c_str(), KEY_READ);
    if (!hklm_key.ReadValueDW(google_update::kRegUsageStatsField, &value))
      return false;
  }
  return (1 == value);
}

bool GoogleUpdateSettings::SetCollectStatsConsent(bool consented) {
  DWORD value = consented? 1 : 0;
  // Writing to HKLM is only a best effort deal.
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring reg_path = dist->GetStateMediumKey();
  RegKey key_hklm(HKEY_LOCAL_MACHINE, reg_path.c_str(), KEY_READ | KEY_WRITE);
  key_hklm.WriteValue(google_update::kRegUsageStatsField, value);
  // Writing to HKCU is used both by chrome and by the crash reporter.
  reg_path = dist->GetStateKey();
  RegKey key_hkcu(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ | KEY_WRITE);
  return key_hkcu.WriteValue(google_update::kRegUsageStatsField, value);
}

bool GoogleUpdateSettings::GetMetricsId(std::wstring* metrics_id) {
  return ReadGoogleUpdateStrKey(google_update::kRegMetricsId, metrics_id);
}

bool GoogleUpdateSettings::SetMetricsId(const std::wstring& metrics_id) {
  return WriteGoogleUpdateStrKey(google_update::kRegMetricsId, metrics_id);
}

bool GoogleUpdateSettings::SetEULAConsent(bool consented) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring reg_path = dist->GetStateMediumKey();
  RegKey key(HKEY_LOCAL_MACHINE, reg_path.c_str(), KEY_READ | KEY_SET_VALUE);
  return key.WriteValue(google_update::kRegEULAAceptedField, consented? 1 : 0);
}

int GoogleUpdateSettings::GetLastRunTime() {
  std::wstring time_s;
  if (!ReadGoogleUpdateStrKey(google_update::kRegLastRunTimeField, &time_s))
    return -1;
  int64 time_i;
  if (!base::StringToInt64(time_s, &time_i))
    return -1;
  base::TimeDelta td =
      base::Time::NowFromSystemTime() - base::Time::FromInternalValue(time_i);
  return td.InDays();
}

bool GoogleUpdateSettings::SetLastRunTime() {
  int64 time = base::Time::NowFromSystemTime().ToInternalValue();
  return WriteGoogleUpdateStrKey(google_update::kRegLastRunTimeField,
                                 base::Int64ToString16(time));
}

bool GoogleUpdateSettings::RemoveLastRunTime() {
  return RemoveGoogleUpdateStrKey(google_update::kRegLastRunTimeField);
}

bool GoogleUpdateSettings::GetBrowser(std::wstring* browser) {
  return ReadGoogleUpdateStrKey(google_update::kRegBrowserField, browser);
}

bool GoogleUpdateSettings::GetLanguage(std::wstring* language) {
  return ReadGoogleUpdateStrKey(google_update::kRegLangField, language);
}

bool GoogleUpdateSettings::GetBrand(std::wstring* brand) {
  return ReadGoogleUpdateStrKey(google_update::kRegRLZBrandField, brand);
}

bool GoogleUpdateSettings::GetClient(std::wstring* client) {
  return ReadGoogleUpdateStrKey(google_update::kRegClientField, client);
}

bool GoogleUpdateSettings::SetClient(const std::wstring& client) {
  return WriteGoogleUpdateStrKey(google_update::kRegClientField, client);
}

bool GoogleUpdateSettings::GetReferral(std::wstring* referral) {
  return ReadGoogleUpdateStrKey(google_update::kRegReferralField, referral);
}

bool GoogleUpdateSettings::ClearReferral() {
  return ClearGoogleUpdateStrKey(google_update::kRegReferralField);
}

bool GoogleUpdateSettings::GetChromeChannel(bool system_install,
    std::wstring* channel) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (dist->GetChromeChannel(channel))
    return true;

  HKEY root_key = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  std::wstring reg_path = dist->GetStateKey();
  RegKey key(root_key, reg_path.c_str(), KEY_READ);
  std::wstring update_branch;
  if (!key.ReadValue(google_update::kRegApField, &update_branch)) {
    *channel = L"unknown";
    return false;
  }

  // Map to something pithy for human consumption. There are no rules as to
  // what the ap string can contain, but generally it will contain a number
  // followed by a dash followed by the branch name (and then some random
  // suffix). We fall back on empty string in case we fail to parse.
  // Only ever return "", "unknown", "dev" or "beta".
  if (update_branch.find(L"-beta") != std::wstring::npos)
    *channel = L"beta";
  else if (update_branch.find(L"-dev") != std::wstring::npos)
    *channel = L"dev";
  else if (update_branch.empty())
    *channel = L"";
  else
    *channel = L"unknown";

  return true;
}

void GoogleUpdateSettings::UpdateDiffInstallStatus(bool system_install,
    bool incremental_install, int install_return_code,
    const std::wstring& product_guid) {
  HKEY reg_root = (system_install) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

  RegKey key;
  std::wstring ap_key_value;
  std::wstring reg_key(google_update::kRegPathClientState);
  reg_key.append(L"\\");
  reg_key.append(product_guid);
  if (!key.Open(reg_root, reg_key.c_str(), KEY_ALL_ACCESS) ||
      !key.ReadValue(google_update::kRegApField, &ap_key_value)) {
    LOG(INFO) << "Application key not found.";
    if (!incremental_install || !install_return_code) {
      LOG(INFO) << "Returning without changing application key.";
      key.Close();
      return;
    } else if (!key.Valid()) {
      reg_key.assign(google_update::kRegPathClientState);
      if (!key.Open(reg_root, reg_key.c_str(), KEY_ALL_ACCESS) ||
          !key.CreateKey(product_guid.c_str(), KEY_ALL_ACCESS)) {
        LOG(ERROR) << "Failed to create application key.";
        key.Close();
        return;
      }
    }
  }

  std::wstring new_value = GetNewGoogleUpdateApKey(
      incremental_install, install_return_code, ap_key_value);
  if ((new_value.compare(ap_key_value) != 0) &&
      !key.WriteValue(google_update::kRegApField, new_value.c_str())) {
    LOG(ERROR) << "Failed to write value " << new_value
               << " to the registry field " << google_update::kRegApField;
  }
  key.Close();
}

std::wstring GoogleUpdateSettings::GetNewGoogleUpdateApKey(
    bool diff_install, int install_return_code, const std::wstring& value) {
  // Magic suffix that we need to add or remove to "ap" key value.
  const std::wstring kMagicSuffix = L"-full";

  bool has_magic_string = false;
  if ((value.length() >= kMagicSuffix.length()) &&
      (value.rfind(kMagicSuffix) == (value.length() - kMagicSuffix.length()))) {
    LOG(INFO) << "Incremental installer failure key already set.";
    has_magic_string = true;
  }

  std::wstring new_value(value);
  if ((!diff_install || !install_return_code) && has_magic_string) {
    LOG(INFO) << "Removing failure key from value " << value;
    new_value = value.substr(0, value.length() - kMagicSuffix.length());
  } else if ((diff_install && install_return_code) &&
             !has_magic_string) {
    LOG(INFO) << "Incremental installer failed, setting failure key.";
    new_value.append(kMagicSuffix);
  }

  return new_value;
}

int GoogleUpdateSettings::DuplicateGoogleUpdateSystemClientKey() {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring reg_path = dist->GetStateKey();

  // Minimum access needed is to be able to write to this key.
  RegKey reg_key(HKEY_LOCAL_MACHINE, reg_path.c_str(), KEY_SET_VALUE);
  if (!reg_key.Valid())
    return 0;

  HANDLE target_handle = 0;
  if (!DuplicateHandle(GetCurrentProcess(), reg_key.Handle(),
                       GetCurrentProcess(), &target_handle, KEY_SET_VALUE,
                       TRUE, DUPLICATE_SAME_ACCESS)) {
    return 0;
  }
  return reinterpret_cast<int>(target_handle);
}

bool GoogleUpdateSettings::WriteGoogleUpdateSystemClientKey(
    int handle, const std::wstring& key, const std::wstring& value) {
  HKEY reg_key = reinterpret_cast<HKEY>(reinterpret_cast<void*>(handle));
  DWORD size = static_cast<DWORD>(value.size()) * sizeof(wchar_t);
  LSTATUS status = RegSetValueEx(reg_key, key.c_str(), 0, REG_SZ,
      reinterpret_cast<const BYTE*>(value.c_str()), size);
  return status == ERROR_SUCCESS;
}

bool GoogleUpdateSettings::IsOrganic(const std::wstring& brand) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kOrganicInstall))
    return true;

  static const wchar_t* kBrands[] = {
      L"CHFO", L"CHFT", L"CHHS", L"CHHM", L"CHMA", L"CHMB", L"CHME", L"CHMF",
      L"CHMG", L"CHMH", L"CHMI", L"CHMQ", L"CHMV", L"CHNB", L"CHNC", L"CHNG",
      L"CHNH", L"CHNI", L"CHOA", L"CHOB", L"CHOC", L"CHON", L"CHOO", L"CHOP",
      L"CHOQ", L"CHOR", L"CHOS", L"CHOT", L"CHOU", L"CHOX", L"CHOY", L"CHOZ",
      L"CHPD", L"CHPE", L"CHPF", L"CHPG", L"EUBB", L"EUBC", L"GGLA", L"GGLS"
  };
  const wchar_t** end = &kBrands[arraysize(kBrands)];
  const wchar_t** found = std::find(&kBrands[0], end, brand);
  if (found != end)
    return true;
  if (StartsWith(brand, L"EUB", true) || StartsWith(brand, L"EUC", true) ||
      StartsWith(brand, L"GGR", true))
    return true;
  return false;
}
