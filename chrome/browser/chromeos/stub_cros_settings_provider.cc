// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/stub_cros_settings_provider.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/login/user_manager.h"

namespace chromeos {

namespace {

const char* kHandledSettings[] = {
  kAccountsPrefAllowGuest,
  kAccountsPrefAllowNewUser,
  kAccountsPrefShowUserNamesOnSignIn,
  kAccountsPrefUsers,
  kAccountsPrefEphemeralUsersEnabled,
  kDeviceOwner,
  kPolicyMissingMitigationMode,
  kReleaseChannel,
  kReportDeviceVersionInfo,
  kReportDeviceActivityTimes,
  kReportDeviceBootMode,
  kReportDeviceLocation,
  kSettingProxyEverywhere,
  kSignedDataRoamingEnabled,
  kStatsReportingPref,
  // Kiosk mode settings.
  kIdleLogoutTimeout,
  kIdleLogoutWarningDuration,
  kScreenSaverExtensionId,
  kScreenSaverTimeout
};

}  // namespace

StubCrosSettingsProvider::StubCrosSettingsProvider(
    const NotifyObserversCallback& notify_cb)
    : CrosSettingsProvider(notify_cb) {
  SetDefaults();
}

StubCrosSettingsProvider::StubCrosSettingsProvider()
  : CrosSettingsProvider(CrosSettingsProvider::NotifyObserversCallback()) {
  SetDefaults();
}

StubCrosSettingsProvider::~StubCrosSettingsProvider() {
}

const base::Value* StubCrosSettingsProvider::Get(
    const std::string& path) const {
  DCHECK(HandlesSetting(path));
  const base::Value* value;
  if (values_.GetValue(path, &value))
    return value;
  return NULL;
}

bool StubCrosSettingsProvider::PrepareTrustedValues(const base::Closure& cb) {
  // We don't have a trusted store so all values are available immediately.
  return true;
}

bool StubCrosSettingsProvider::HandlesSetting(const std::string& path) const {
  const char** end = kHandledSettings + arraysize(kHandledSettings);
  return std::find(kHandledSettings, end, path) != end;
}

void StubCrosSettingsProvider::Reload() {
}

void StubCrosSettingsProvider::DoSet(const std::string& path,
                                     const base::Value& value) {
  values_.SetValue(path, value.DeepCopy());
  NotifyObservers(path);
}

void StubCrosSettingsProvider::SetDefaults() {
  values_.SetBoolean(kAccountsPrefAllowGuest, true);
  values_.SetBoolean(kAccountsPrefAllowNewUser, true);
  values_.SetBoolean(kAccountsPrefShowUserNamesOnSignIn, true);
  // |kDeviceOwner| will be set to the logged-in user by |UserManager|.
}

}  // namespace chromeos
