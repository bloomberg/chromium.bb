
// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/updater_state_win.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"

#include "base/win/registry.h"
#include "base/win/win_util.h"

namespace component_updater {

namespace {

// Google Update group policy settings.
const wchar_t kGoogleUpdatePoliciesKey[] =
    L"SOFTWARE\\Policies\\Google\\Update";
const wchar_t kCheckPeriodOverrideMinutes[] = L"AutoUpdateCheckPeriodMinutes";
const wchar_t kUpdatePolicyValue[] = L"UpdateDefault";
const wchar_t kChromeUpdatePolicyOverride[] =
    L"Update{8A69D345-D564-463C-AFF1-A69D9E530F96}";

// Don't allow update periods longer than six weeks (Chrome release cadence).
const int kCheckPeriodOverrideMinutesMax = 60 * 24 * 7 * 6;

// Google Update registry settings.
const wchar_t kRegPathGoogleUpdate[] = L"Software\\Google\\Update";
const wchar_t kRegPathClientsGoogleUpdate[] =
    L"Software\\Google\\Update\\Clients\\"
    L"{430FD4D0-B729-4F61-AA34-91526481799D}";
const wchar_t kRegValueGoogleUpdatePv[] = L"pv";
const wchar_t kRegValueLastStartedAU[] = L"LastStartedAU";
const wchar_t kRegValueLastChecked[] = L"LastChecked";

}  // namespace

UpdaterState::UpdaterState(bool is_machine) : is_machine_(is_machine) {}

std::unique_ptr<UpdaterState> UpdaterState::Create(bool is_machine) {
  std::unique_ptr<UpdaterState> updater_state(new UpdaterState(is_machine));
  updater_state->ReadState();
  return updater_state;
}

void UpdaterState::ReadState() {
  google_update_version_ = GetGoogleUpdateVersion(is_machine_);
  last_autoupdate_started_ = GetGoogleUpdateLastStartedAU(is_machine_);
  last_checked_ = GetGoogleUpdateLastChecked(is_machine_);
  is_joined_to_domain_ = IsJoinedToDomain();
  is_autoupdate_check_enabled_ = IsAutoupdateCheckEnabled();
  chrome_update_policy_ = GetChromeUpdatePolicy();
}

update_client::InstallerAttributes UpdaterState::MakeInstallerAttributes()
    const {
  update_client::InstallerAttributes installer_attributes;

  if (google_update_version_.IsValid())
    installer_attributes["googleupdatever"] =
        google_update_version_.GetString();

  const base::Time now = base::Time::NowFromSystemTime();
  if (!last_autoupdate_started_.is_null())
    installer_attributes["laststarted"] =
        NormalizeTimeDelta(now - last_autoupdate_started_);
  if (!last_checked_.is_null())
    installer_attributes["lastchecked"] =
        NormalizeTimeDelta(now - last_checked_);

  installer_attributes["domainjoined"] = is_joined_to_domain_ ? "1" : "0";
  installer_attributes["autoupdatecheckenabled"] =
      is_autoupdate_check_enabled_ ? "1" : "0";

  DCHECK(chrome_update_policy_ >= 0 && chrome_update_policy_ <= 3 ||
         chrome_update_policy_ == -1);
  installer_attributes["chromeupdatepolicy"] =
      base::IntToString(chrome_update_policy_);

  return installer_attributes;
}

std::string UpdaterState::NormalizeTimeDelta(const base::TimeDelta& delta) {
  const base::TimeDelta two_weeks = base::TimeDelta::FromDays(14);
  const base::TimeDelta two_months = base::TimeDelta::FromDays(60);

  std::string val;  // Contains the value to return in hours.
  if (delta <= two_weeks) {
    val = "0";
  } else if (two_weeks < delta && delta <= two_months) {
    val = "408";  // 2 weeks in hours.
  } else {
    val = "1344";  // 2*28 days in hours.
  }

  DCHECK(!val.empty());
  return val;
}

base::Version UpdaterState::GetGoogleUpdateVersion(bool is_machine) {
  const HKEY root_key = is_machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  base::string16 version;
  base::win::RegKey key;

  if (key.Open(root_key, kRegPathClientsGoogleUpdate,
               KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS &&
      key.ReadValue(kRegValueGoogleUpdatePv, &version) == ERROR_SUCCESS) {
    return base::Version(base::UTF16ToUTF8(version));
  }

  return base::Version();
}

base::Time UpdaterState::GetGoogleUpdateLastStartedAU(bool is_machine) {
  return GetGoogleUpdateTimeValue(is_machine, kRegValueLastStartedAU);
}

base::Time UpdaterState::GetGoogleUpdateLastChecked(bool is_machine) {
  return GetGoogleUpdateTimeValue(is_machine, kRegValueLastChecked);
}

base::Time UpdaterState::GetGoogleUpdateTimeValue(bool is_machine,
                                                  const wchar_t* value_name) {
  const HKEY root_key = is_machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  base::win::RegKey update_key;

  if (update_key.Open(root_key, kRegPathGoogleUpdate,
                      KEY_QUERY_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    DWORD value(0);
    if (update_key.ReadValueDW(value_name, &value) == ERROR_SUCCESS) {
      return base::Time::FromTimeT(value);
    }
  }

  return base::Time();
}

bool UpdaterState::IsAutoupdateCheckEnabled() {
  // Check the auto-update check period override. If it is 0 or exceeds the
  // maximum timeout, then for all intents and purposes auto updates are
  // disabled.
  base::win::RegKey policy_key;
  DWORD value = 0;
  if (policy_key.Open(HKEY_LOCAL_MACHINE, kGoogleUpdatePoliciesKey,
                      KEY_QUERY_VALUE) == ERROR_SUCCESS &&
      policy_key.ReadValueDW(kCheckPeriodOverrideMinutes, &value) ==
          ERROR_SUCCESS &&
      (value == 0 || value > kCheckPeriodOverrideMinutesMax)) {
    return false;
  }

  return true;
}

// Returns -1 if the policy is not found or the value was invalid. Otherwise,
// returns a value in the [0, 3] range, representing the value of the
// Chrome update group policy.
int UpdaterState::GetChromeUpdatePolicy() {
  const int kMaxUpdatePolicyValue = 3;

  base::win::RegKey policy_key;

  if (policy_key.Open(HKEY_LOCAL_MACHINE, kGoogleUpdatePoliciesKey,
                      KEY_QUERY_VALUE) != ERROR_SUCCESS) {
    return -1;
  }

  DWORD value = 0;
  // First try to read the Chrome-specific override.
  if (policy_key.ReadValueDW(kChromeUpdatePolicyOverride, &value) ==
          ERROR_SUCCESS &&
      value <= kMaxUpdatePolicyValue) {
    return value;
  }

  // Try to read default override.
  if (policy_key.ReadValueDW(kUpdatePolicyValue, &value) == ERROR_SUCCESS &&
      value <= kMaxUpdatePolicyValue) {
    return value;
  }

  return -1;
}

bool UpdaterState::IsJoinedToDomain() {
  return base::win::IsEnrolledToDomain();
}

}  // namespace component_updater
