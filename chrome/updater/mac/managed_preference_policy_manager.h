// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_MAC_MANAGED_PREFERENCE_POLICY_MANAGER_H_
#define CHROME_UPDATER_MAC_MANAGED_PREFERENCE_POLICY_MANAGER_H_

#include <memory>

#include "chrome/updater/policy_manager.h"

namespace updater {

// A factory method to create a managed preference policy manager.
std::unique_ptr<PolicyManagerInterface> CreateManagedPreferencePolicyManager();

}  // namespace updater

#endif  // CHROME_UPDATER_MAC_MANAGED_PREFERENCE_POLICY_MANAGER_H_
