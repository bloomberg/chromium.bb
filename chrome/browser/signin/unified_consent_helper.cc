// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/unified_consent_helper.h"

#include "base/feature_list.h"
#include "build/buildflag.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "components/signin/core/browser/signin_buildflags.h"
#include "components/unified_consent/feature.h"

namespace {

unified_consent::UnifiedConsentFeatureState GetUnifiedConsentFeatureState(
    Profile* profile) {
  DCHECK(profile);
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // On Dice platforms, unified consent requires Dice to be enabled first.
  if (!AccountConsistencyModeManager::IsDiceEnabledForProfile(profile))
    return unified_consent::UnifiedConsentFeatureState::kDisabled;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  return unified_consent::internal::GetUnifiedConsentFeatureState();
}

}  // namespace

bool IsUnifiedConsentEnabled(Profile* profile) {
  DCHECK(profile);
  unified_consent::UnifiedConsentFeatureState feature_state =
      GetUnifiedConsentFeatureState(profile);
  return feature_state !=
         unified_consent::UnifiedConsentFeatureState::kDisabled;
}

bool IsUnifiedConsentBumpEnabled(Profile* profile) {
  DCHECK(profile);
  unified_consent::UnifiedConsentFeatureState feature_state =
      GetUnifiedConsentFeatureState(profile);
  return feature_state ==
         unified_consent::UnifiedConsentFeatureState::kEnabledWithBump;
}
