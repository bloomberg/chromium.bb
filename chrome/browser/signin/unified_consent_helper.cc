// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/unified_consent_helper.h"

#include "base/feature_list.h"
#include "build/buildflag.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/signin_buildflags.h"

bool IsUnifiedConsentEnabled(Profile* profile) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // On Dice platforms, unified consent requires Dice to be enabled first.
  if (!AccountConsistencyModeManager::IsDiceEnabledForProfile(profile))
    return false;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  return base::FeatureList::IsEnabled(signin::kUnifiedConsent);
}
