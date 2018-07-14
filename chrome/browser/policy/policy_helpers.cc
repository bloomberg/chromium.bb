// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_helpers.h"

#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

namespace policy {

void RegisterPrefs(PrefRegistrySimple* registry) {
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  registry->RegisterBooleanPref(prefs::kCloudPolicyOverridesMachinePolicy,
                                false);
#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
}

}  // namespace policy
