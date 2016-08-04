// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"

#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/common/chrome_features.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {

namespace {
bool enable_for_testing_ = false;
}  // namespace

bool IsQuickUnlockEnabled() {
  if (enable_for_testing_)
    return true;

  // TODO(jdufault): Implement a proper policy check. For now, just disable if
  // the device is enterprise enrolled. See crbug.com/612271.
  if (g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->IsEnterpriseManaged()) {
    return false;
  }

  // TODO(jdufault): Disable PIN for supervised users until we allow the owner
  // to set the PIN. See crbug.com/632797.
  user_manager::User* user = user_manager::UserManager::Get()->GetActiveUser();
  if (user && user->IsSupervised())
    return false;

  // Enable quick unlock only if the switch is present.
  return base::FeatureList::IsEnabled(features::kQuickUnlockPin);
}

void EnableQuickUnlockForTesting() {
  enable_for_testing_ = true;
}

}  // namespace chromeos
