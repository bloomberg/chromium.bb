// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/common/profile_management_switches.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "build/build_config.h"
#include "components/signin/core/common/signin_switches.h"

namespace switches {

bool IsEnableAccountConsistency() {
#if defined(OS_ANDROID) || defined(OS_IOS)
  // Account consistency is enabled on Android and iOS.
  return true;
#endif

  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableAccountConsistency);
}

bool IsExtensionsMultiAccount() {
#if defined(OS_ANDROID) || defined(OS_IOS)
  NOTREACHED() << "Extensions are not enabled on Android or iOS";
  // Account consistency is enabled on Android and iOS.
  return false;
#endif

  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kExtensionsMultiAccount) ||
         IsEnableAccountConsistency();
}

bool IsNewProfileManagement() {
  return true;
}

bool UsePasswordSeparatedSigninFlow() {
  return base::FeatureList::IsEnabled(
      switches::kUsePasswordSeparatedSigninFlow);
}

void EnableNewProfileManagementForTesting(base::CommandLine* command_line) {
  // No-op as new profile management is always enabled.
}

void EnableAccountConsistencyForTesting(base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kEnableAccountConsistency);
}

}  // namespace switches
