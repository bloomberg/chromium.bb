// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace chromeos {

namespace multidevice_setup {

// Note that the pref names have slightly inconsistent naming conventions
// because some were named before the unified MultiDeviceSetup project and we
// wanted to avoid changing the internal names of existing prefs. The general
// naming pattern for each individual feature enabling pref moving forward
// should be of the form
//     const char k[FeatureName]FeatureEnabledPrefName =
//         "multidevice_setup.[feature_name]_enabled";

// This pref is a gatekeeper for all MultiDevice features (e.g. Easy Unlock,
// Instant Tethering). Setting the pref to 'true' is necessary but not
// sufficient to enable the individual features, which are each controlled by
// their own pref and may involve additional setup steps.
const char kSuiteEnabledPrefName[] = "multidevice_setup.suite_enabled";

// Individual feature prefs.
const char kAndroidMessagesFeatureEnabledPrefName[] =
    "multidevice.sms_connect_enabled";

// Whether Instant Tethering is allowed by policy.
const char kInstantTetheringFeatureAllowedPrefName[] = "tether.allowed";

// Whether Instant Tethering is enabled by the user. Note that if the feature is
// enabled by the user but disallowed by policy, it is unavailable.
const char kInstantTetheringFeatureEnabledPrefName[] = "tether.enabled";

void RegisterFeaturePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kSuiteEnabledPrefName, true);
  registry->RegisterBooleanPref(kAndroidMessagesFeatureEnabledPrefName, true);
  registry->RegisterBooleanPref(kInstantTetheringFeatureAllowedPrefName, true);
  registry->RegisterBooleanPref(kInstantTetheringFeatureEnabledPrefName, true);
}

}  // namespace multidevice_setup

}  // namespace chromeos
