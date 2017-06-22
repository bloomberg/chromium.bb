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

AccountConsistencyMethod GetAccountConsistencyMethod() {
#if BUILDFLAG(ENABLE_MIRROR)
  // Mirror is enabled on Android and iOS.
  return AccountConsistencyMethod::kMirror;
#else
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  std::string method = cmd->GetSwitchValueASCII(switches::kAccountConsistency);
  if (method == switches::kAccountConsistencyMirror)
    return AccountConsistencyMethod::kMirror;

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  if (method == switches::kAccountConsistencyDice)
    return AccountConsistencyMethod::kDice;
#endif

  return AccountConsistencyMethod::kDisabled;
#endif  // BUILDFLAG(ENABLE_MIRROR)
}

bool IsAccountConsistencyMirrorEnabled() {
  return GetAccountConsistencyMethod() == AccountConsistencyMethod::kMirror;
}

bool IsAccountConsistencyDiceEnabled() {
  return GetAccountConsistencyMethod() == AccountConsistencyMethod::kDice;
}

bool IsExtensionsMultiAccount() {
#if defined(OS_ANDROID) || defined(OS_IOS)
  NOTREACHED() << "Extensions are not enabled on Android or iOS";
  // Account consistency is enabled on Android and iOS.
  return false;
#endif

  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kExtensionsMultiAccount) ||
         GetAccountConsistencyMethod() == AccountConsistencyMethod::kMirror;
}

void EnableAccountConsistencyMirrorForTesting(base::CommandLine* command_line) {
#if !BUILDFLAG(ENABLE_MIRROR)
  command_line->AppendSwitchASCII(switches::kAccountConsistency,
                                  switches::kAccountConsistencyMirror);
#endif
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
void EnableAccountConsistencyDiceForTesting(base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(switches::kAccountConsistency,
                                  switches::kAccountConsistencyDice);
}
#endif

}  // namespace switches
