// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/unified_consent/feature.h"

#include "components/unified_consent/feature.h"

bool IsUnifiedConsentFeatureEnabled() {
  unified_consent::UnifiedConsentFeatureState feature_state =
      unified_consent::internal::GetUnifiedConsentFeatureState();
  return feature_state !=
         unified_consent::UnifiedConsentFeatureState::kDisabled;
}

bool IsUnifiedConsentFeatureWithBumpEnabled() {
  unified_consent::UnifiedConsentFeatureState feature_state =
      unified_consent::internal::GetUnifiedConsentFeatureState();
  return feature_state ==
         unified_consent::UnifiedConsentFeatureState::kEnabledWithBump;
}
