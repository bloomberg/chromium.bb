// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/common/profile_management_switches.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "components/signin/core/common/signin_features.h"
#include "components/signin/core/common/signin_switches.h"

namespace signin {

// base::Feature definitions.
const base::Feature kAccountConsistencyFeature{
    "AccountConsistency", base::FEATURE_DISABLED_BY_DEFAULT};
const char kAccountConsistencyFeatureMethodParameter[] = "method";
const char kAccountConsistencyFeatureMethodMirror[] = "mirror";
const char kAccountConsistencyFeatureMethodDiceFixAuthErrors[] =
    "dice_fix_auth_errors";
const char kAccountConsistencyFeatureMethodDiceMigration[] = "dice_migration";
const char kAccountConsistencyFeatureMethodDice[] = "dice";

AccountConsistencyMethod GetAccountConsistencyMethod() {
#if BUILDFLAG(ENABLE_MIRROR)
  // Mirror is always enabled on Android and iOS.
  return AccountConsistencyMethod::kMirror;
#else
  if (!base::FeatureList::IsEnabled(kAccountConsistencyFeature))
    return AccountConsistencyMethod::kDisabled;

  std::string method_value = base::GetFieldTrialParamValueByFeature(
      kAccountConsistencyFeature, kAccountConsistencyFeatureMethodParameter);

  if (method_value == kAccountConsistencyFeatureMethodMirror)
    return AccountConsistencyMethod::kMirror;
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  else if (method_value == kAccountConsistencyFeatureMethodDiceFixAuthErrors)
    return AccountConsistencyMethod::kDiceFixAuthErrors;
  else if (method_value == kAccountConsistencyFeatureMethodDiceMigration)
    return AccountConsistencyMethod::kDiceMigration;
  else if (method_value == kAccountConsistencyFeatureMethodDice)
    return AccountConsistencyMethod::kDice;
#endif

  return AccountConsistencyMethod::kDisabled;
#endif  // BUILDFLAG(ENABLE_MIRROR)
}

bool IsAccountConsistencyMirrorEnabled() {
  return GetAccountConsistencyMethod() == AccountConsistencyMethod::kMirror;
}

bool IsDiceMigrationEnabled() {
  return (GetAccountConsistencyMethod() ==
          AccountConsistencyMethod::kDiceMigration) ||
         (GetAccountConsistencyMethod() == AccountConsistencyMethod::kDice);
}

bool IsDiceFixAuthErrorsEnabled() {
  AccountConsistencyMethod method = GetAccountConsistencyMethod();
  return (method == AccountConsistencyMethod::kDiceFixAuthErrors) ||
         (method == AccountConsistencyMethod::kDiceMigration) ||
         (method == AccountConsistencyMethod::kDice);
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

}  // namespace signin
