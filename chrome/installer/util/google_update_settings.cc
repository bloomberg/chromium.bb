// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/google_update_settings.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/app_registration_data.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/channel_info.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_experiment_util.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/product.h"

using base::win::RegKey;
using installer::InstallationState;

const wchar_t GoogleUpdateSettings::kPoliciesKey[] =
    L"SOFTWARE\\Policies\\Google\\Update";
const wchar_t GoogleUpdateSettings::kUpdatePolicyValue[] = L"UpdateDefault";
const wchar_t GoogleUpdateSettings::kUpdateOverrideValuePrefix[] = L"Update";
const wchar_t GoogleUpdateSettings::kCheckPeriodOverrideMinutes[] =
    L"AutoUpdateCheckPeriodMinutes";

// Don't allow update periods longer than six weeks.
const int GoogleUpdateSettings::kCheckPeriodOverrideMinutesMax =
    60 * 24 * 7 * 6;

const GoogleUpdateSettings::UpdatePolicy
GoogleUpdateSettings::kDefaultUpdatePolicy =
#if defined(GOOGLE_CHROME_BUILD)
    GoogleUpdateSettings::AUTOMATIC_UPDATES;
#else
    GoogleUpdateSettings::UPDATES_DISABLED;
#endif

namespace {

bool ReadGoogleUpdateStrKey(const wchar_t* const name, base::string16* value) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  base::string16 reg_path = dist->GetStateKey();
  RegKey key(HKEY_CURRENT_USER, reg_path.c_str(), KEY_READ | KEY_WOW64_32KEY);
  if (key.ReadValue(name, value) != ERROR_SUCCESS) {
    RegKey hklm_key(
        HKEY_LOCAL_MACHINE, reg_path.c_str(), KEY_READ | KEY_WOW64_32KEY);
    return (hklm_key.ReadValue(name, value) == ERROR_SUCCESS);
  }
  return true;
}

// Updates a registry key |name| to be |value| for the given |app_reg_data|.
// If this is a |system_install|, then update the value under HKLM (istead of
// HKCU for user-installs) using a group of keys (one for each OS user) and also
// include the method to |aggregate| these values when reporting.
bool WriteGoogleUpdateStrKeyInternal(const AppRegistrationData& app_reg_data,
                                     bool system_install,
                                     const wchar_t* const name,
                                     const base::string16& value,
                                     const wchar_t* const aggregate) {
  const REGSAM kAccess = KEY_SET_VALUE | KEY_WOW64_32KEY;
  if (system_install) {
    DCHECK(aggregate);
    // Machine installs require each OS user to write a unique key under a
    // named key in HKLM as well as an "aggregation" function that describes
    // how the values of multiple users are to be combined.
    base::string16 uniquename;
    if (!base::win::GetUserSidString(&uniquename)) {
      NOTREACHED();
      return false;
    }

    base::string16 reg_path(app_reg_data.GetStateMediumKey());
    reg_path.append(L"\\");
    reg_path.append(name);
    RegKey key(HKEY_LOCAL_MACHINE, reg_path.c_str(), kAccess);
    key.WriteValue(google_update::kRegAggregateMethod, aggregate);
    return (key.WriteValue(uniquename.c_str(), value.c_str()) == ERROR_SUCCESS);
  } else {
    // User installs are easy: just write the values to HKCU tree.
    RegKey key(HKEY_CURRENT_USER, app_reg_data.GetStateKey().c_str(), kAccess);
    return (key.WriteValue(name, value.c_str()) == ERROR_SUCCESS);
  }
}

bool WriteGoogleUpdateStrKey(const wchar_t* const name,
                             const base::string16& value) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  return WriteGoogleUpdateStrKeyInternal(
      dist->GetAppRegistrationData(), false, name, value, NULL);
}

bool ClearGoogleUpdateStrKey(const wchar_t* const name) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  base::string16 reg_path = dist->GetStateKey();
  RegKey key(HKEY_CURRENT_USER,
             reg_path.c_str(),
             KEY_READ | KEY_WRITE | KEY_WOW64_32KEY);
  base::string16 value;
  if (key.ReadValue(name, &value) != ERROR_SUCCESS)
    return false;
  return (key.WriteValue(name, L"") == ERROR_SUCCESS);
}

bool RemoveGoogleUpdateStrKey(const wchar_t* const name) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  base::string16 reg_path = dist->GetStateKey();
  RegKey key(HKEY_CURRENT_USER,
             reg_path.c_str(),
             KEY_READ | KEY_WRITE | KEY_WOW64_32KEY);
  if (!key.HasValue(name))
    return true;
  return (key.DeleteValue(name) == ERROR_SUCCESS);
}

bool GetChromeChannelInternal(bool system_install,
                              bool add_multi_modifier,
                              base::string16* channel) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();

  // Shortcut in case this distribution knows what channel it is (canary).
  if (dist->GetChromeChannel(channel))
    return true;

  // Determine whether or not chrome is multi-install. If so, updates are
  // delivered under the binaries' app guid, so that's where the relevant
  // channel is found.
  installer::ProductState state;
  installer::ChannelInfo channel_info;
  ignore_result(state.Initialize(system_install, dist));
  if (!state.is_multi_install()) {
    // Use the channel info that was just read for this single-install chrome.
    channel_info = state.channel();
  } else {
    // Read the channel info from the binaries' state key.
    HKEY root_key = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    dist = BrowserDistribution::GetSpecificDistribution(
        BrowserDistribution::CHROME_BINARIES);
    RegKey key(root_key, dist->GetStateKey().c_str(),
               KEY_READ | KEY_WOW64_32KEY);

    if (!channel_info.Initialize(key)) {
      channel->assign(installer::kChromeChannelUnknown);
      return false;
    }
  }

  if (!channel_info.GetChannelName(channel))
    channel->assign(installer::kChromeChannelUnknown);

  // Tag the channel name if this is a multi-install.
  if (add_multi_modifier && state.is_multi_install()) {
    if (!channel->empty())
      channel->push_back(L'-');
    channel->push_back(L'm');
  }

  return true;
}

// Populates |update_policy| with the UpdatePolicy enum value corresponding to a
// DWORD read from the registry and returns true if |value| is within range.
// If |value| is out of range, returns false without modifying |update_policy|.
bool GetUpdatePolicyFromDword(
    const DWORD value,
    GoogleUpdateSettings::UpdatePolicy* update_policy) {
  switch (value) {
    case GoogleUpdateSettings::UPDATES_DISABLED:
    case GoogleUpdateSettings::AUTOMATIC_UPDATES:
    case GoogleUpdateSettings::MANUAL_UPDATES_ONLY:
    case GoogleUpdateSettings::AUTO_UPDATES_ONLY:
      *update_policy = static_cast<GoogleUpdateSettings::UpdatePolicy>(value);
      return true;
    default:
      LOG(WARNING) << "Unexpected update policy override value: " << value;
  }
  return false;
}

// Convenience routine: GoogleUpdateSettings::UpdateDidRunStateForApp()
// specialized for Chrome Binaries.
bool UpdateDidRunStateForBinaries(bool did_run) {
  BrowserDistribution* dist = BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_BINARIES);
  return GoogleUpdateSettings::UpdateDidRunStateForApp(
      dist->GetAppRegistrationData(), did_run);
}

}  // namespace

bool GoogleUpdateSettings::IsSystemInstall() {
  bool system_install = false;
  base::FilePath module_dir;
  if (!PathService::Get(base::DIR_MODULE, &module_dir)) {
    LOG(WARNING)
        << "Failed to get directory of module; assuming per-user install.";
  } else {
    system_install = !InstallUtil::IsPerUserInstall(module_dir.value().c_str());
  }
  return system_install;
}

bool GoogleUpdateSettings::GetCollectStatsConsent() {
  return GetCollectStatsConsentAtLevel(IsSystemInstall());
}

// Older versions of Chrome unconditionally read from HKCU\...\ClientState\...
// and then HKLM\...\ClientState\....  This means that system-level Chrome
// never checked ClientStateMedium (which has priority according to Google
// Update) and gave preference to a value in HKCU (which was never checked by
// Google Update).  From now on, Chrome follows Google Update's policy.
bool GoogleUpdateSettings::GetCollectStatsConsentAtLevel(bool system_install) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();

  // Consent applies to all products in a multi-install package.
  if (InstallUtil::IsMultiInstall(dist, system_install)) {
    dist = BrowserDistribution::GetSpecificDistribution(
        BrowserDistribution::CHROME_BINARIES);
  }

  RegKey key;
  DWORD value = 0;
  bool have_value = false;
  const REGSAM kAccess = KEY_QUERY_VALUE | KEY_WOW64_32KEY;

  // For system-level installs, try ClientStateMedium first.
  have_value =
      system_install &&
      key.Open(HKEY_LOCAL_MACHINE, dist->GetStateMediumKey().c_str(),
               kAccess) == ERROR_SUCCESS &&
      key.ReadValueDW(google_update::kRegUsageStatsField,
                      &value) == ERROR_SUCCESS;

  // Otherwise, try ClientState.
  if (!have_value) {
    have_value =
        key.Open(system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER,
                 dist->GetStateKey().c_str(),
                 kAccess) == ERROR_SUCCESS &&
        key.ReadValueDW(google_update::kRegUsageStatsField,
                        &value) == ERROR_SUCCESS;
  }

  // Google Update specifically checks that the value is 1, so we do the same.
  return have_value && value == 1;
}

bool GoogleUpdateSettings::SetCollectStatsConsent(bool consented) {
  return SetCollectStatsConsentAtLevel(IsSystemInstall(), consented);
}

bool GoogleUpdateSettings::SetCollectStatsConsentAtLevel(bool system_install,
                                                         bool consented) {
  // Google Update writes and expects 1 for true, 0 for false.
  DWORD value = consented ? 1 : 0;

  BrowserDistribution* dist = BrowserDistribution::GetDistribution();

  // Consent applies to all products in a multi-install package.
  if (InstallUtil::IsMultiInstall(dist, system_install)) {
    dist = BrowserDistribution::GetSpecificDistribution(
        BrowserDistribution::CHROME_BINARIES);
  }

  // Write to ClientStateMedium for system-level; ClientState otherwise.
  HKEY root_key = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  base::string16 reg_path =
      system_install ? dist->GetStateMediumKey() : dist->GetStateKey();
  RegKey key;
  LONG result = key.Create(
      root_key, reg_path.c_str(), KEY_SET_VALUE | KEY_WOW64_32KEY);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed opening key " << reg_path << " to set "
               << google_update::kRegUsageStatsField << "; result: " << result;
  } else {
    result = key.WriteValue(google_update::kRegUsageStatsField, value);
    LOG_IF(ERROR, result != ERROR_SUCCESS) << "Failed setting "
        << google_update::kRegUsageStatsField << " in key " << reg_path
        << "; result: " << result;
  }
  return (result == ERROR_SUCCESS);
}

scoped_ptr<metrics::ClientInfo> GoogleUpdateSettings::LoadMetricsClientInfo() {
  base::string16 client_id_16;
  if (!ReadGoogleUpdateStrKey(google_update::kRegMetricsId, &client_id_16) ||
      client_id_16.empty()) {
    return scoped_ptr<metrics::ClientInfo>();
  }

  scoped_ptr<metrics::ClientInfo> client_info(new metrics::ClientInfo);
  client_info->client_id = base::UTF16ToUTF8(client_id_16);

  base::string16 installation_date_str;
  if (ReadGoogleUpdateStrKey(google_update::kRegMetricsIdInstallDate,
                             &installation_date_str)) {
    base::StringToInt64(installation_date_str, &client_info->installation_date);
  }

  base::string16 reporting_enbaled_date_date_str;
  if (ReadGoogleUpdateStrKey(google_update::kRegMetricsIdEnabledDate,
                             &reporting_enbaled_date_date_str)) {
    base::StringToInt64(reporting_enbaled_date_date_str,
                        &client_info->reporting_enabled_date);
  }

  return client_info.Pass();
}

void GoogleUpdateSettings::StoreMetricsClientInfo(
    const metrics::ClientInfo& client_info) {
  // Attempt a best-effort at backing |client_info| in the registry (but don't
  // handle/report failures).
  WriteGoogleUpdateStrKey(google_update::kRegMetricsId,
                          base::UTF8ToUTF16(client_info.client_id));
  WriteGoogleUpdateStrKey(google_update::kRegMetricsIdInstallDate,
                          base::Int64ToString16(client_info.installation_date));
  WriteGoogleUpdateStrKey(
      google_update::kRegMetricsIdEnabledDate,
      base::Int64ToString16(client_info.reporting_enabled_date));
}

// EULA consent is only relevant for system-level installs.
bool GoogleUpdateSettings::SetEULAConsent(
    const InstallationState& machine_state,
    BrowserDistribution* dist,
    bool consented) {
  DCHECK(dist);
  const DWORD eula_accepted = consented ? 1 : 0;
  const REGSAM kAccess = KEY_SET_VALUE | KEY_WOW64_32KEY;
  base::string16 reg_path = dist->GetStateMediumKey();
  bool succeeded = true;
  RegKey key;

  // Write the consent value into the product's ClientStateMedium key.
  if (key.Create(HKEY_LOCAL_MACHINE, reg_path.c_str(),
                 kAccess) != ERROR_SUCCESS ||
      key.WriteValue(google_update::kRegEULAAceptedField,
                     eula_accepted) != ERROR_SUCCESS) {
    succeeded = false;
  }

  // If this is a multi-install, also write it into the binaries' key.
  // --mutli-install is not provided on the command-line, so deduce it from
  // the product's state.
  const installer::ProductState* product_state =
      machine_state.GetProductState(true, dist->GetType());
  if (product_state != NULL && product_state->is_multi_install()) {
    dist = BrowserDistribution::GetSpecificDistribution(
        BrowserDistribution::CHROME_BINARIES);
    reg_path = dist->GetStateMediumKey();
    if (key.Create(HKEY_LOCAL_MACHINE, reg_path.c_str(),
                   kAccess) != ERROR_SUCCESS ||
        key.WriteValue(google_update::kRegEULAAceptedField,
                       eula_accepted) != ERROR_SUCCESS) {
        succeeded = false;
    }
  }

  return succeeded;
}

int GoogleUpdateSettings::GetLastRunTime() {
  base::string16 time_s;
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

bool GoogleUpdateSettings::GetBrowser(base::string16* browser) {
  return ReadGoogleUpdateStrKey(google_update::kRegBrowserField, browser);
}

bool GoogleUpdateSettings::GetLanguage(base::string16* language) {
  return ReadGoogleUpdateStrKey(google_update::kRegLangField, language);
}

bool GoogleUpdateSettings::GetBrand(base::string16* brand) {
  return ReadGoogleUpdateStrKey(google_update::kRegRLZBrandField, brand);
}

bool GoogleUpdateSettings::GetReactivationBrand(base::string16* brand) {
  return ReadGoogleUpdateStrKey(google_update::kRegRLZReactivationBrandField,
                                brand);
}

bool GoogleUpdateSettings::GetClient(base::string16* client) {
  return ReadGoogleUpdateStrKey(google_update::kRegClientField, client);
}

bool GoogleUpdateSettings::SetClient(const base::string16& client) {
  return WriteGoogleUpdateStrKey(google_update::kRegClientField, client);
}

bool GoogleUpdateSettings::GetReferral(base::string16* referral) {
  return ReadGoogleUpdateStrKey(google_update::kRegReferralField, referral);
}

bool GoogleUpdateSettings::ClearReferral() {
  return ClearGoogleUpdateStrKey(google_update::kRegReferralField);
}

bool GoogleUpdateSettings::UpdateDidRunStateForApp(
    const AppRegistrationData& app_reg_data,
    bool did_run) {
  return WriteGoogleUpdateStrKeyInternal(app_reg_data,
                                         false, // user level.
                                         google_update::kRegDidRunField,
                                         did_run ? L"1" : L"0",
                                         NULL);
}

bool GoogleUpdateSettings::UpdateDidRunState(bool did_run, bool system_level) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  bool result = UpdateDidRunStateForApp(dist->GetAppRegistrationData(),
                                        did_run);
  // Update state for binaries, even if the previous call was unsuccessful.
  if (InstallUtil::IsMultiInstall(dist, system_level))
    result = UpdateDidRunStateForBinaries(did_run) && result;
  return result;
}

base::string16 GoogleUpdateSettings::GetChromeChannel(bool system_install) {
  base::string16 channel;
  GetChromeChannelInternal(system_install, false, &channel);
  return channel;
}

bool GoogleUpdateSettings::GetChromeChannelAndModifiers(
    bool system_install,
    base::string16* channel) {
  return GetChromeChannelInternal(system_install, true, channel);
}

void GoogleUpdateSettings::UpdateInstallStatus(bool system_install,
    installer::ArchiveType archive_type, int install_return_code,
    const base::string16& product_guid) {
  DCHECK(archive_type != installer::UNKNOWN_ARCHIVE_TYPE ||
         install_return_code != 0);
  HKEY reg_root = (system_install) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

  RegKey key;
  installer::ChannelInfo channel_info;
  base::string16 reg_key(google_update::kRegPathClientState);
  reg_key.append(L"\\");
  reg_key.append(product_guid);
  LONG result = key.Open(reg_root,
                         reg_key.c_str(),
                         KEY_QUERY_VALUE | KEY_SET_VALUE | KEY_WOW64_32KEY);
  if (result == ERROR_SUCCESS)
    channel_info.Initialize(key);
  else if (result != ERROR_FILE_NOT_FOUND)
    LOG(ERROR) << "Failed to open " << reg_key << "; Error: " << result;

  if (UpdateGoogleUpdateApKey(archive_type, install_return_code,
                              &channel_info)) {
    // We have a modified channel_info value to write.
    // Create the app's ClientState key if it doesn't already exist.
    if (!key.Valid()) {
      result = key.Open(reg_root,
                        google_update::kRegPathClientState,
                        KEY_CREATE_SUB_KEY | KEY_WOW64_32KEY);
      if (result == ERROR_SUCCESS)
        result = key.CreateKey(product_guid.c_str(),
                               KEY_SET_VALUE | KEY_WOW64_32KEY);

      if (result != ERROR_SUCCESS) {
        LOG(ERROR) << "Failed to create " << reg_key << "; Error: " << result;
        return;
      }
    }
    if (!channel_info.Write(&key)) {
      LOG(ERROR) << "Failed to write to application's ClientState key "
                 << google_update::kRegApField << " = " << channel_info.value();
    }
  }
}

bool GoogleUpdateSettings::UpdateGoogleUpdateApKey(
    installer::ArchiveType archive_type, int install_return_code,
    installer::ChannelInfo* value) {
  DCHECK(archive_type != installer::UNKNOWN_ARCHIVE_TYPE ||
         install_return_code != 0);
  bool modified = false;

  if (archive_type == installer::FULL_ARCHIVE_TYPE || !install_return_code) {
    if (value->SetFullSuffix(false)) {
      VLOG(1) << "Removed incremental installer failure key; "
                 "switching to channel: "
              << value->value();
      modified = true;
    }
  } else if (archive_type == installer::INCREMENTAL_ARCHIVE_TYPE) {
    if (value->SetFullSuffix(true)) {
      VLOG(1) << "Incremental installer failed; switching to channel: "
              << value->value();
      modified = true;
    } else {
      VLOG(1) << "Incremental installer failure; already on channel: "
              << value->value();
    }
  } else {
    // It's okay if we don't know the archive type.  In this case, leave the
    // "-full" suffix as we found it.
    DCHECK_EQ(installer::UNKNOWN_ARCHIVE_TYPE, archive_type);
  }

  if (value->SetMultiFailSuffix(false)) {
    VLOG(1) << "Removed multi-install failure key; switching to channel: "
            << value->value();
    modified = true;
  }

  return modified;
}

void GoogleUpdateSettings::UpdateProfileCounts(int profiles_active,
                                               int profiles_signedin) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  bool system_install = IsSystemInstall();
  WriteGoogleUpdateStrKeyInternal(dist->GetAppRegistrationData(),
                                  system_install,
                                  google_update::kRegProfilesActive,
                                  base::Int64ToString16(profiles_active),
                                  L"sum()");
  WriteGoogleUpdateStrKeyInternal(dist->GetAppRegistrationData(),
                                  system_install,
                                  google_update::kRegProfilesSignedIn,
                                  base::Int64ToString16(profiles_signedin),
                                  L"sum()");
}

int GoogleUpdateSettings::DuplicateGoogleUpdateSystemClientKey() {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  base::string16 reg_path = dist->GetStateKey();

  // Minimum access needed is to be able to write to this key.
  RegKey reg_key(
      HKEY_LOCAL_MACHINE, reg_path.c_str(), KEY_SET_VALUE | KEY_WOW64_32KEY);
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
    int handle, const base::string16& key, const base::string16& value) {
  HKEY reg_key = reinterpret_cast<HKEY>(reinterpret_cast<void*>(handle));
  DWORD size = static_cast<DWORD>(value.size()) * sizeof(wchar_t);
  LSTATUS status = RegSetValueEx(reg_key, key.c_str(), 0, REG_SZ,
      reinterpret_cast<const BYTE*>(value.c_str()), size);
  return status == ERROR_SUCCESS;
}

GoogleUpdateSettings::UpdatePolicy GoogleUpdateSettings::GetAppUpdatePolicy(
    const base::string16& app_guid,
    bool* is_overridden) {
  bool found_override = false;
  UpdatePolicy update_policy = kDefaultUpdatePolicy;

#if defined(GOOGLE_CHROME_BUILD)
  DCHECK(!app_guid.empty());
  RegKey policy_key;

  // Google Update Group Policy settings are always in HKLM.
  // TODO(wfh): Check if policies should go into Wow6432Node or not.
  if (policy_key.Open(HKEY_LOCAL_MACHINE, kPoliciesKey, KEY_QUERY_VALUE) ==
          ERROR_SUCCESS) {
    DWORD value = 0;
    base::string16 app_update_override(kUpdateOverrideValuePrefix);
    app_update_override.append(app_guid);
    // First try to read and comprehend the app-specific override.
    found_override = (policy_key.ReadValueDW(app_update_override.c_str(),
                                             &value) == ERROR_SUCCESS &&
                      GetUpdatePolicyFromDword(value, &update_policy));

    // Failing that, try to read and comprehend the default override.
    if (!found_override &&
        policy_key.ReadValueDW(kUpdatePolicyValue, &value) == ERROR_SUCCESS) {
      GetUpdatePolicyFromDword(value, &update_policy);
    }
  }
#endif  // defined(GOOGLE_CHROME_BUILD)

  if (is_overridden != NULL)
    *is_overridden = found_override;

  return update_policy;
}

// static
bool GoogleUpdateSettings::AreAutoupdatesEnabled() {
#if defined(GOOGLE_CHROME_BUILD)
  // Check the auto-update check period override. If it is 0 or exceeds the
  // maximum timeout, then for all intents and purposes auto updates are
  // disabled.
  RegKey policy_key;
  DWORD value = 0;
  if (policy_key.Open(HKEY_LOCAL_MACHINE, kPoliciesKey,
                      KEY_QUERY_VALUE) == ERROR_SUCCESS &&
      policy_key.ReadValueDW(kCheckPeriodOverrideMinutes,
                             &value) == ERROR_SUCCESS &&
      (value == 0 || value > kCheckPeriodOverrideMinutesMax)) {
    return false;
  }

  // Auto updates are subtly broken when Chrome and the binaries have different
  // overrides in place. If this Chrome cannot possibly be multi-install by
  // virtue of being a side-by-side installation, simply check Chrome's policy.
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  UpdatePolicy app_policy = GetAppUpdatePolicy(dist->GetAppGuid(), nullptr);
  if (InstallUtil::IsChromeSxSProcess())
    return app_policy == AUTOMATIC_UPDATES || app_policy == AUTO_UPDATES_ONLY;

  // Otherwise, check for consistency between Chrome and the binaries regardless
  // of whether or not this Chrome is multi-install since the next update likely
  // will attempt to migrate it to such.
  BrowserDistribution* binaries = BrowserDistribution::GetSpecificDistribution(
      BrowserDistribution::CHROME_BINARIES);
  return (GetAppUpdatePolicy(binaries->GetAppGuid(), nullptr) == app_policy &&
          (app_policy == AUTOMATIC_UPDATES || app_policy == AUTO_UPDATES_ONLY));
#else  // defined(GOOGLE_CHROME_BUILD)
  // Chromium does not auto update.
  return false;
#endif  // !defined(GOOGLE_CHROME_BUILD)
}

// static
bool GoogleUpdateSettings::ReenableAutoupdates() {
#if defined(GOOGLE_CHROME_BUILD)
  int needs_reset_count = 0;
  int did_reset_count = 0;

  // Reset overrides for Chrome and for the binaries if this Chrome supports
  // multi-install.
  std::vector<base::string16> app_guids;
  app_guids.push_back(BrowserDistribution::GetDistribution()->GetAppGuid());
  if (!InstallUtil::IsChromeSxSProcess()) {
    app_guids.push_back(BrowserDistribution::GetSpecificDistribution(
        BrowserDistribution::CHROME_BINARIES)->GetAppGuid());
  }

  UpdatePolicy update_policy = kDefaultUpdatePolicy;
  RegKey policy_key;
  if (policy_key.Open(HKEY_LOCAL_MACHINE, kPoliciesKey,
                      KEY_SET_VALUE | KEY_QUERY_VALUE) == ERROR_SUCCESS) {
    // Set to true while app-specific overrides are present that allow automatic
    // updates. When this is the case, the defaults are irrelevant and don't
    // need to be checked or reset.
    bool automatic_updates_allowed_by_overrides = true;
    DWORD value = 0;
    for (const base::string16& app_guid : app_guids) {
      // First check the app-specific override value and reset that if needed.
      // Note that this intentionally sets the override to AUTOMATIC_UPDATES
      // even if it was previously AUTO_UPDATES_ONLY. The thinking is that
      // AUTOMATIC_UPDATES is marginally more likely to let a user update and
      // this code is only called when a stuck user asks for updates.
      base::string16 app_update_override(kUpdateOverrideValuePrefix);
      app_update_override.append(app_guid);
      if (policy_key.ReadValueDW(app_update_override.c_str(),
                                 &value) != ERROR_SUCCESS) {
        automatic_updates_allowed_by_overrides = false;
      } else if (!GetUpdatePolicyFromDword(value, &update_policy) ||
                 update_policy != GoogleUpdateSettings::AUTOMATIC_UPDATES) {
        automatic_updates_allowed_by_overrides = false;
        ++needs_reset_count;
        if (policy_key.WriteValue(
                app_update_override.c_str(),
                static_cast<DWORD>(GoogleUpdateSettings::AUTOMATIC_UPDATES)) ==
            ERROR_SUCCESS) {
          ++did_reset_count;
        }
      }
    }

    // If there were no app-specific override policies, see if there's a global
    // policy preventing updates and delete it if so.
    if (!automatic_updates_allowed_by_overrides &&
        policy_key.ReadValueDW(kUpdatePolicyValue, &value) == ERROR_SUCCESS &&
        (!GetUpdatePolicyFromDword(value, &update_policy) ||
         update_policy != GoogleUpdateSettings::AUTOMATIC_UPDATES)) {
      ++needs_reset_count;
      if (policy_key.DeleteValue(kUpdatePolicyValue) == ERROR_SUCCESS)
        ++did_reset_count;
    }

    // Check the auto-update check period override. If it is 0 or exceeds
    // the maximum timeout, delete the override value.
    if (policy_key.ReadValueDW(kCheckPeriodOverrideMinutes,
                               &value) == ERROR_SUCCESS &&
        (value == 0 || value > kCheckPeriodOverrideMinutesMax)) {
      ++needs_reset_count;
      if (policy_key.DeleteValue(kCheckPeriodOverrideMinutes) == ERROR_SUCCESS)
        ++did_reset_count;
    }

    // Return whether the number of successful resets is the same as the
    // number of things that appeared to need resetting.
    return (needs_reset_count == did_reset_count);
  } else {
    // For some reason we couldn't open the policy key with the desired
    // permissions to make changes (the most likely reason is that there is no
    // policy set). Simply return whether or not we think updates are enabled.
    return AreAutoupdatesEnabled();
  }

#endif
  // Non Google Chrome isn't going to autoupdate.
  return true;
}

void GoogleUpdateSettings::RecordChromeUpdatePolicyHistograms() {
  const bool is_multi_install = InstallUtil::IsMultiInstall(
      BrowserDistribution::GetDistribution(), IsSystemInstall());
  const base::string16 app_guid =
      BrowserDistribution::GetSpecificDistribution(
          is_multi_install ? BrowserDistribution::CHROME_BINARIES :
                             BrowserDistribution::CHROME_BROWSER)->GetAppGuid();

  bool is_overridden = false;
  const UpdatePolicy update_policy = GetAppUpdatePolicy(app_guid,
                                                        &is_overridden);
  UMA_HISTOGRAM_BOOLEAN("GoogleUpdate.UpdatePolicyIsOverridden", is_overridden);
  UMA_HISTOGRAM_ENUMERATION("GoogleUpdate.EffectivePolicy", update_policy,
                            UPDATE_POLICIES_COUNT);
}

base::string16 GoogleUpdateSettings::GetUninstallCommandLine(
    bool system_install) {
  const HKEY root_key = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  base::string16 cmd_line;
  RegKey update_key;

  if (update_key.Open(root_key, google_update::kRegPathGoogleUpdate,
                      KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    update_key.ReadValue(google_update::kRegUninstallCmdLine, &cmd_line);
  }

  return cmd_line;
}

Version GoogleUpdateSettings::GetGoogleUpdateVersion(bool system_install) {
  const HKEY root_key = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  base::string16 version;
  RegKey key;

  if (key.Open(root_key,
               google_update::kRegPathGoogleUpdate,
               KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS &&
      key.ReadValue(google_update::kRegGoogleUpdateVersion, &version) ==
          ERROR_SUCCESS) {
    return Version(base::UTF16ToUTF8(version));
  }

  return Version();
}

base::Time GoogleUpdateSettings::GetGoogleUpdateLastStartedAU(
    bool system_install) {
  const HKEY root_key = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  RegKey update_key;

  if (update_key.Open(root_key,
                      google_update::kRegPathGoogleUpdate,
                      KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    DWORD last_start;
    if (update_key.ReadValueDW(google_update::kRegLastStartedAUField,
                               &last_start) == ERROR_SUCCESS) {
      return base::Time::FromTimeT(last_start);
    }
  }

  return base::Time();
}

base::Time GoogleUpdateSettings::GetGoogleUpdateLastChecked(
    bool system_install) {
  const HKEY root_key = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  RegKey update_key;

  if (update_key.Open(root_key,
                      google_update::kRegPathGoogleUpdate,
                      KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    DWORD last_check;
    if (update_key.ReadValueDW(google_update::kRegLastCheckedField,
                               &last_check) == ERROR_SUCCESS) {
      return base::Time::FromTimeT(last_check);
    }
  }

  return base::Time();
}

bool GoogleUpdateSettings::GetUpdateDetailForApp(bool system_install,
                                                 const wchar_t* app_guid,
                                                 ProductData* data) {
  DCHECK(app_guid);
  DCHECK(data);

  bool product_found = false;

  const HKEY root_key = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  base::string16 clientstate_reg_path(google_update::kRegPathClientState);
  clientstate_reg_path.append(L"\\");
  clientstate_reg_path.append(app_guid);

  RegKey clientstate;
  if (clientstate.Open(root_key,
                       clientstate_reg_path.c_str(),
                       KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    base::string16 version;
    DWORD dword_value;
    if ((clientstate.ReadValueDW(google_update::kRegLastCheckSuccessField,
                                 &dword_value) == ERROR_SUCCESS) &&
        (clientstate.ReadValue(google_update::kRegVersionField,
                               &version) == ERROR_SUCCESS)) {
      product_found = true;
      data->version = base::UTF16ToASCII(version);
      data->last_success = base::Time::FromTimeT(dword_value);
      data->last_result = 0;
      data->last_error_code = 0;
      data->last_extra_code = 0;

      if (clientstate.ReadValueDW(google_update::kRegLastInstallerResultField,
                                  &dword_value) == ERROR_SUCCESS) {
        // Google Update convention is that if an installer writes an result
        // code that is invalid, it is clamped to an exit code result.
        const DWORD kMaxValidInstallResult = 4;  // INSTALLER_RESULT_EXIT_CODE
        data->last_result = std::min(dword_value, kMaxValidInstallResult);
      }
      if (clientstate.ReadValueDW(google_update::kRegLastInstallerErrorField,
                                  &dword_value) == ERROR_SUCCESS) {
        data->last_error_code = dword_value;
      }
      if (clientstate.ReadValueDW(google_update::kRegLastInstallerExtraField,
                                  &dword_value) == ERROR_SUCCESS) {
        data->last_extra_code = dword_value;
      }
    }
  }

  return product_found;
}

bool GoogleUpdateSettings::GetUpdateDetailForGoogleUpdate(bool system_install,
                                                          ProductData* data) {
  return GetUpdateDetailForApp(system_install,
                               google_update::kGoogleUpdateUpgradeCode,
                               data);
}

bool GoogleUpdateSettings::GetUpdateDetail(bool system_install,
                                           ProductData* data) {
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  return GetUpdateDetailForApp(system_install,
                               dist->GetAppGuid().c_str(),
                               data);
}

bool GoogleUpdateSettings::SetExperimentLabels(
    bool system_install,
    const base::string16& experiment_labels) {
  HKEY reg_root = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

  // Use the browser distribution and install level to write to the correct
  // client state/app guid key.
  bool success = false;
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (dist->ShouldSetExperimentLabels()) {
    base::string16 client_state_path(
        system_install ? dist->GetStateMediumKey() : dist->GetStateKey());
    RegKey client_state(
        reg_root, client_state_path.c_str(), KEY_SET_VALUE | KEY_WOW64_32KEY);
    if (experiment_labels.empty()) {
      success = client_state.DeleteValue(google_update::kExperimentLabels)
          == ERROR_SUCCESS;
    } else {
      success = client_state.WriteValue(google_update::kExperimentLabels,
          experiment_labels.c_str()) == ERROR_SUCCESS;
    }
  }

  return success;
}

bool GoogleUpdateSettings::ReadExperimentLabels(
    bool system_install,
    base::string16* experiment_labels) {
  HKEY reg_root = system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

  // If this distribution does not set the experiment labels, don't bother
  // reading.
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (!dist->ShouldSetExperimentLabels())
    return false;

  base::string16 client_state_path(
      system_install ? dist->GetStateMediumKey() : dist->GetStateKey());

  RegKey client_state;
  LONG result = client_state.Open(
      reg_root, client_state_path.c_str(), KEY_QUERY_VALUE | KEY_WOW64_32KEY);
  if (result == ERROR_SUCCESS) {
    result = client_state.ReadValue(google_update::kExperimentLabels,
                                    experiment_labels);
  }

  // If the key or value was not present, return the empty string.
  if (result == ERROR_FILE_NOT_FOUND || result == ERROR_PATH_NOT_FOUND) {
    experiment_labels->clear();
    return true;
  }

  return result == ERROR_SUCCESS;
}
