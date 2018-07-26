// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"

#include "components/prefs/pref_registry_simple.h"

namespace chromeos {

namespace multidevice_setup {

const char kMultiDeviceSuiteEnabledPrefName[] =
    "multidevice_setup.enable_feature_suite";

void RegisterFeaturePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kMultiDeviceSuiteEnabledPrefName, false);
}

}  // namespace multidevice_setup

}  // namespace chromeos
