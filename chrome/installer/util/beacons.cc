// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/beacons.h"

#include <stdint.h>

#include "base/memory/ptr_util.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/installer/util/app_registration_data.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"

void UpdateDefaultBrowserBeaconForPath(const base::FilePath& chrome_exe) {
  // Getting Chrome's default state causes the beacon to be updated via a call
  // to UpdateDefaultBrowserBeaconWithState below.
  ignore_result(ShellUtil::GetChromeDefaultStateFromPath(chrome_exe));
}

void UpdateDefaultBrowserBeaconWithState(
    BrowserDistribution* distribution,
    ShellUtil::DefaultState default_state) {
  const bool system_install = !InstallUtil::IsPerUserInstall();
  const AppRegistrationData& registration_data =
      distribution->GetAppRegistrationData();
  switch (default_state) {
    case ShellUtil::NOT_DEFAULT:
      installer_util::MakeFirstNotDefaultBeacon(system_install,
                                                registration_data)->Update();
      break;
    case ShellUtil::IS_DEFAULT:
      installer_util::MakeLastWasDefaultBeacon(system_install,
                                               registration_data)->Update();
      installer_util::MakeFirstNotDefaultBeacon(system_install,
                                                registration_data)->Remove();
      break;
    case ShellUtil::UNKNOWN_DEFAULT:
      break;
  }
}

void UpdateOsUpgradeBeacon(bool system_install,
                           BrowserDistribution* distribution) {
  installer_util::MakeLastOsUpgradeBeacon(
      system_install, distribution->GetAppRegistrationData())->Update();
}

namespace installer_util {

std::unique_ptr<Beacon> MakeLastOsUpgradeBeacon(
    bool system_install,
    const AppRegistrationData& registration_data) {
  return base::MakeUnique<Beacon>(L"LastOsUpgrade", Beacon::BeaconType::LAST,
                                  Beacon::BeaconScope::PER_INSTALL,
                                  system_install, registration_data);
}

std::unique_ptr<Beacon> MakeLastWasDefaultBeacon(
    bool system_install,
    const AppRegistrationData& registration_data) {
  return base::MakeUnique<Beacon>(L"LastWasDefault", Beacon::BeaconType::LAST,
                                  Beacon::BeaconScope::PER_USER, system_install,
                                  registration_data);
}

std::unique_ptr<Beacon> MakeFirstNotDefaultBeacon(
    bool system_install,
    const AppRegistrationData& registration_data) {
  return base::MakeUnique<Beacon>(L"FirstNotDefault", Beacon::BeaconType::FIRST,
                                  Beacon::BeaconScope::PER_USER, system_install,
                                  registration_data);
}

// Beacon ----------------------------------------------------------------------

Beacon::Beacon(base::StringPiece16 name,
               BeaconType type,
               BeaconScope scope,
               bool system_install,
               const AppRegistrationData& registration_data)
    : type_(type),
      root_(system_install ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER),
      scope_(scope) {
  Initialize(name, system_install, registration_data);
}

Beacon::~Beacon() {
}

void Beacon::Update() {
  const REGSAM kAccess = KEY_WOW64_32KEY | KEY_QUERY_VALUE | KEY_SET_VALUE;
  base::win::RegKey key;

  // Nothing to update if the key couldn't be created. This should only be the
  // case for a developer build.
  if (key.Create(root_, key_path_.c_str(), kAccess) != ERROR_SUCCESS)
    return;

  // Nothing to do if this beacon is tracking the first occurrence of an event
  // that has already been recorded.
  if (type_ == BeaconType::FIRST && key.HasValue(value_name_.c_str()))
    return;

  int64_t now(base::Time::Now().ToInternalValue());
  key.WriteValue(value_name_.c_str(), &now, sizeof(now), REG_QWORD);
}

void Beacon::Remove() {
  const REGSAM kAccess = KEY_WOW64_32KEY | KEY_SET_VALUE;
  base::win::RegKey key;

  if (key.Open(root_, key_path_.c_str(), kAccess) == ERROR_SUCCESS)
    key.DeleteValue(value_name_.c_str());
}

base::Time Beacon::Get() {
  const REGSAM kAccess = KEY_WOW64_32KEY | KEY_QUERY_VALUE;
  base::win::RegKey key;
  int64_t now;

  if (key.Open(root_, key_path_.c_str(), kAccess) != ERROR_SUCCESS ||
      key.ReadInt64(value_name_.c_str(), &now) != ERROR_SUCCESS) {
    return base::Time();
  }

  return base::Time::FromInternalValue(now);
}

void Beacon::Initialize(base::StringPiece16 name,
                        bool system_install,
                        const AppRegistrationData& registration_data) {
  // When possible, beacons are located in the app's ClientState key. Per-user
  // beacons for a per-machine install are located in a beacon-specific sub-key
  // of the app's ClientStateMedium key.
  if (!system_install || scope_ == BeaconScope::PER_INSTALL) {
    key_path_ = registration_data.GetStateKey();
    value_name_ = name.as_string();
  } else {
    key_path_ = registration_data.GetStateMediumKey();
    key_path_.push_back(L'\\');
    key_path_.append(name.data(), name.size());
    // This should never fail. If it does, the beacon will be written in the
    // key's default value, which is okay since the majority case is likely a
    // machine with a single user.
    if (!base::win::GetUserSidString(&value_name_))
      NOTREACHED();
  }
}

}  // namespace installer_util
