// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_PREFS_H_
#define CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_PREFS_H_

class PrefRegistrySimple;

namespace chromeos {

namespace multidevice_setup {

// This pref is a gatekeeper for all MultiDevice features (e.g. Easy Unlock,
// Instant Tethering). Setting the pref to 'true' is necessary but not
// sufficient to enable the individual features, which are each controlled by
// their own pref and may involve additional setup steps.
extern const char kMultiDeviceSuiteEnabledPrefName[];

void RegisterFeaturePrefs(PrefRegistrySimple* registry);

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MULTIDEVICE_SETUP_PUBLIC_CPP_PREFS_H_
