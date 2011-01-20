// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/google_update_settings.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "base/win/registry.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/package.h"
#include "chrome/installer/util/package_properties.h"
#include "chrome/installer/util/product.h"

using base::win::RegKey;

namespace {

// An list of search results in increasing order of desirability.
enum EulaSearchResult {
  NO_SETTING,
  FOUND_CLIENT_STATE,
  FOUND_OPPOSITE_SETTING,
  FOUND_SAME_SETTING
};

bool ReadGoogleUpdateStrKey(const wchar_t* const name, std::wstring* value) {
  // The registry functions below will end up going to disk.  Do this on another
  // thread to avoid slowing the IO thread.  http://crbug.com/62121
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring reg_path = dist->GetStateKey();
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ);
  if (key.ReadValue(name, value) != ERROR_SUCCESS) {
    RegKey hklm_key(HKEY_LOCAL_MACHINE, reg_path.c_str(), KEY_READ);
    return (hklm_key.ReadValue(name, value) == ERROR_SUCCESS);
  }
  return true;
}

bool WriteGoogleUpdateStrKey(const wchar_t* const name,
                             const std::wstring& value) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring reg_path = dist->GetStateKey();
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ | KEY_WRITE);
  return (key.WriteValue(name, value.c_str()) == ERROR_SUCCESS);
}

bool ClearGoogleUpdateStrKey(const wchar_t* const name) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring reg_path = dist->GetStateKey();
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ | KEY_WRITE);
  std::wstring value;
  if (key.ReadValue(name, &value) != ERROR_SUCCESS)
    return false;
  return (key.WriteValue(name, L"") == ERROR_SUCCESS);
}

bool RemoveGoogleUpdateStrKey(const wchar_t* const name) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring reg_path = dist->GetStateKey();
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ | KEY_WRITE);
  if (!key.ValueExists(name))
    return true;
  return (key.DeleteValue(name) == ERROR_SUCCESS);
}

EulaSearchResult HasEULASetting(HKEY root, const std::wstring& state_key,
                                bool setting) {
  RegKey key;
  DWORD previous_value = setting ? 1 : 0;
  if (key.Open(root, state_key.c_str(), KEY_QUERY_VALUE) != ERROR_SUCCESS)
    return NO_SETTING;
  if (key.ReadValueDW(google_update::kRegEULAAceptedField,
                      &previous_value) != ERROR_SUCCESS)
    return FOUND_CLIENT_STATE;

  return ((previous_value != 0) == setting) ?
      FOUND_SAME_SETTING : FOUND_OPPOSITE_SETTING;
}

}  // namespace.

bool GoogleUpdateSettings::GetCollectStatsConsent() {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring reg_path = dist->GetStateKey();
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ);
  DWORD value = 0;
  if (key.ReadValueDW(google_update::kRegUsageStatsField, &value) !=
      ERROR_SUCCESS) {
    key.Open(HKEY_LOCAL_MACHINE, reg_path.c_str(), KEY_READ);
    key.ReadValueDW(google_update::kRegUsageStatsField, &value);
  }
  return (1 == value);
}

bool GoogleUpdateSettings::SetCollectStatsConsent(bool consented) {
  DWORD value = consented? 1 : 0;
  // Writing to HKLM is only a best effort deal.
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  std::wstring reg_path = dist->GetStateMediumKey();
  RegKey key(HKEY_LOCAL_MACHINE, reg_path.c_str(), KEY_READ | KEY_WRITE);
  key.WriteValue(google_update::kRegUsageStatsField, value);
  // Writing to HKCU is used both by chrome and by the crash reporter.
  reg_path = dist->GetStateKey();
  key.Create(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ | KEY_WRITE);
  return (key.WriteValue(google_update::kRegUsageStatsField, value) ==
      ERROR_SUCCESS);
}

bool GoogleUpdateSettings::GetMetricsId(std::wstring* metrics_id) {
  return ReadGoogleUpdateStrKey(google_update::kRegMetricsId, metrics_id);
}

bool GoogleUpdateSettings::SetMetricsId(const std::wstring& metrics_id) {
  return WriteGoogleUpdateStrKey(google_update::kRegMetricsId, metrics_id);
}

bool GoogleUpdateSettings::SetEULAConsent(
    const installer::Package& package,
    bool consented) {
  // If this is a multi install, Google Update will have put eulaaccepted=0 into
  // the ClientState key of the multi-installer.  Conduct a brief search for
  // this value and store the consent in the corresponding location.
  HKEY root = package.system_level() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

  std::wstring reg_path = package.properties()->GetStateMediumKey();
  EulaSearchResult status = HasEULASetting(
      root, package.properties()->GetStateKey(), !consented);
  if (status != FOUND_SAME_SETTING) {
    EulaSearchResult new_status = NO_SETTING;
    installer::Products::const_iterator scan = package.products().begin();
    installer::Products::const_iterator end = package.products().end();
    for (; status != FOUND_SAME_SETTING && scan != end; ++scan) {
      const installer::Product& product = *(scan->get());
      new_status = HasEULASetting(root, product.distribution()->GetStateKey(),
                                  !consented);
      if (new_status > status) {
        status = new_status;
        reg_path = product.distribution()->GetStateMediumKey();
      }
    }
    if (status == NO_SETTING) {
      LOG(WARNING)
          << "eulaaccepted value not found; setting consent on package";
      reg_path = package.properties()->GetStateMediumKey();
    }
  }
  RegKey key(HKEY_LOCAL_MACHINE, reg_path.c_str(), KEY_SET_VALUE);
  return (key.WriteValue(google_update::kRegEULAAceptedField,
                         consented ? 1 : 0) == ERROR_SUCCESS);
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
  installer::ChannelInfo channel_info;
  if (!channel_info.Initialize(key)) {
    *channel = L"unknown";
    return false;
  }

  if (!channel_info.GetChannelName(channel))
    *channel = L"unknown";

  // Tag the channel name if this is a multi-install product.
  if (channel_info.IsMultiInstall()) {
    if (!channel->empty())
      channel->append(1, L'-');
    channel->append(1, L'm');
  }

  return true;
}

void GoogleUpdateSettings::UpdateInstallStatus(bool system_install,
    bool incremental_install, bool multi_install, int install_return_code,
    const std::wstring& product_guid) {
  HKEY reg_root = (system_install) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

  RegKey key;
  installer::ChannelInfo channel_info;
  std::wstring reg_key(google_update::kRegPathClientState);
  reg_key.append(L"\\");
  reg_key.append(product_guid);
  LONG result = key.Open(reg_root, reg_key.c_str(),
                         KEY_QUERY_VALUE | KEY_SET_VALUE);
  if (result != ERROR_SUCCESS || !channel_info.Initialize(key)) {
    VLOG(1) << "Application key not found.";
    if (!incremental_install && !multi_install || !install_return_code) {
      VLOG(1) << "Returning without changing application key.";
      return;
    } else if (!key.Valid()) {
      reg_key.assign(google_update::kRegPathClientState);
      result = key.Open(reg_root, reg_key.c_str(), KEY_CREATE_SUB_KEY);
      if (result == ERROR_SUCCESS)
        result = key.CreateKey(product_guid.c_str(), KEY_SET_VALUE);

      if (result != ERROR_SUCCESS) {
        LOG(ERROR) << "Failed to create application key. Error: " << result;
        return;
      }
    }
  }

  if (UpdateGoogleUpdateApKey(incremental_install, multi_install,
                              install_return_code, &channel_info) &&
      !channel_info.Write(&key)) {
    LOG(ERROR) << "Failed to write value " << channel_info.value()
               << " to the registry field " << google_update::kRegApField;
  }
}

bool GoogleUpdateSettings::UpdateGoogleUpdateApKey(
    bool diff_install, bool multi_install, int install_return_code,
    installer::ChannelInfo* value) {
  bool modified = false;

  if (!diff_install || !install_return_code) {
    if (value->SetFullSuffix(false)) {
      VLOG(1) << "Removed incremental installer failure key; "
                 "switching to channel: "
              << value->value();
      modified = true;
    }
  } else {
    if (value->SetFullSuffix(true)) {
      VLOG(1) << "Incremental installer failed; switching to channel: "
              << value->value();
      modified = true;
    } else {
      VLOG(1) << "Incremental installer failure; already on channel: "
              << value->value();
    }
  }

  if (!multi_install || !install_return_code) {
    if (value->SetMultiFailSuffix(false)) {
      VLOG(1) << "Removed multi-install failure key; switching to channel: "
              << value->value();
      modified = true;
    }
  } else {
    if (value->SetMultiFailSuffix(true)) {
      VLOG(1) << "Multi-install failed; switching to channel: "
              << value->value();
      modified = true;
    } else {
      VLOG(1) << "Multi-install failed; already on channel: " << value->value();
    }
  }

  return modified;
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
