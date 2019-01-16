// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"

#include <string>

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"

namespace plugin_vm {

bool IsPluginVmAllowedForProfile(Profile* profile) {
  // Check that the profile is eligible.
  if (!profile || profile->IsChild() || profile->IsLegacySupervised() ||
      profile->IsOffTheRecord() ||
      chromeos::ProfileHelper::IsEphemeralUserProfile(profile) ||
      chromeos::ProfileHelper::IsLockScreenAppProfile(profile)) {
    return false;
  }

  // Check that the user is affiliated.
  const user_manager::User* const user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (user == nullptr || !user->IsAffiliated())
    return false;

  // Check that PluginVm is allowed to run by policy.
  bool plugin_vm_allowed_for_device;
  if (!chromeos::CrosSettings::Get()->GetBoolean(
          chromeos::kPluginVmAllowed, &plugin_vm_allowed_for_device)) {
    return false;
  }
  if (!plugin_vm_allowed_for_device)
    return false;

  // Check that a license key is set.
  std::string plugin_vm_license_key;
  if (!chromeos::CrosSettings::Get()->GetString(chromeos::kPluginVmLicenseKey,
                                                &plugin_vm_license_key)) {
    return false;
  }
  if (plugin_vm_license_key == std::string())
    return false;

  // Check that a VM image is set.
  if (!profile->GetPrefs()->HasPrefPath(plugin_vm::prefs::kPluginVmImage))
    return false;

  return true;
}

bool IsPluginVmConfigured(Profile* profile) {
  if (!profile->GetPrefs()->GetBoolean(
          plugin_vm::prefs::kPluginVmImageExists)) {
    return false;
  }
  return true;
}

}  // namespace plugin_vm
