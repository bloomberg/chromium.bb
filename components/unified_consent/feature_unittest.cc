// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/feature.h"

#include "components/sync/driver/sync_driver_switches.h"
#include "components/unified_consent/scoped_unified_consent.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unified_consent {

TEST(UnifiedConsentFeatureTest, FeatureState) {
  // Unified consent is disabled by default.
  EXPECT_FALSE(IsUnifiedConsentFeatureEnabled());
  EXPECT_FALSE(IsUnifiedConsentFeatureWithBumpEnabled());

  {
    ScopedUnifiedConsent scoped_disabled(UnifiedConsentFeatureState::kDisabled);
    EXPECT_FALSE(IsUnifiedConsentFeatureEnabled());
    EXPECT_FALSE(IsUnifiedConsentFeatureWithBumpEnabled());
  }

  {
    ScopedUnifiedConsent scoped_no_bump(
        UnifiedConsentFeatureState::kEnabledNoBump);
    EXPECT_TRUE(IsUnifiedConsentFeatureEnabled());
    EXPECT_FALSE(IsUnifiedConsentFeatureWithBumpEnabled());
  }

  {
    ScopedUnifiedConsent scoped_bump(
        UnifiedConsentFeatureState::kEnabledWithBump);
    EXPECT_TRUE(IsUnifiedConsentFeatureEnabled());
    EXPECT_TRUE(IsUnifiedConsentFeatureWithBumpEnabled());
  }
}

}  // namespace unified_consent
