// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace multidevice_setup {

// Note: Pref name strings follow an inconsistent naming convention because some
// of them were created before the MultiDeviceSetup project.

// "Allowed by user policy" preferences:
const char kInstantTetheringAllowedPrefName[] = "tether.allowed";
const char kSmartLockAllowedPrefName[] = "easy_unlock.allowed";

// "Enabled by user" preferences:
const char kBetterTogetherSuiteEnabledPrefName[] =
    "multidevice_setup.suite_enabled";
const char kInstantTetheringEnabledPrefName[] = "tether.enabled";
const char kMessagesEnabledPrefName[] = "multidevice.sms_connect_enabled";
const char kSmartLockEnabledPrefName[] = "easy_unlock.enabled";

void RegisterFeaturePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kInstantTetheringAllowedPrefName, true);
  // TODO(khorimoto): Register "messages allowed" preference.
  registry->RegisterBooleanPref(kSmartLockAllowedPrefName, true);

  registry->RegisterBooleanPref(kBetterTogetherSuiteEnabledPrefName, true);
  registry->RegisterBooleanPref(kInstantTetheringEnabledPrefName, true);
  registry->RegisterBooleanPref(kMessagesEnabledPrefName, true);
  registry->RegisterBooleanPref(kSmartLockEnabledPrefName, true);
}

bool AreAnyMultiDeviceFeaturesAllowed(PrefService* pref_service) {
  // TODO(khorimoto): Read from "messages allowed" preference when available.
  return pref_service->GetBoolean(kInstantTetheringAllowedPrefName) ||
         pref_service->GetBoolean(kSmartLockAllowedPrefName);
}

}  // namespace multidevice_setup

}  // namespace chromeos
