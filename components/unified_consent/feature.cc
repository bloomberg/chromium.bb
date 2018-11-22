// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/feature.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/sync/driver/sync_driver_switches.h"

namespace unified_consent {

// base::Feature definitions.
const base::Feature kUnifiedConsent{"UnifiedConsent",
                                    base::FEATURE_DISABLED_BY_DEFAULT};
const char kUnifiedConsentShowBumpParameter[] = "show_consent_bump";
const base::Feature kForceUnifiedConsentBump{"ForceUnifiedConsentBump",
                                             base::FEATURE_DISABLED_BY_DEFAULT};

namespace {
UnifiedConsentFeatureState GetUnifiedConsentFeatureState() {
  if (!base::FeatureList::IsEnabled(kUnifiedConsent))
    return UnifiedConsentFeatureState::kDisabled;

  std::string show_bump = base::GetFieldTrialParamValueByFeature(
      kUnifiedConsent, kUnifiedConsentShowBumpParameter);
  return show_bump.empty() ? UnifiedConsentFeatureState::kEnabledNoBump
                           : UnifiedConsentFeatureState::kEnabledWithBump;
}
}  // namespace

bool IsUnifiedConsentFeatureEnabled() {
  return GetUnifiedConsentFeatureState() !=
         UnifiedConsentFeatureState::kDisabled;
}

bool IsUnifiedConsentFeatureWithBumpEnabled() {
  return GetUnifiedConsentFeatureState() ==
         UnifiedConsentFeatureState::kEnabledWithBump;
}

}  // namespace unified_consent
