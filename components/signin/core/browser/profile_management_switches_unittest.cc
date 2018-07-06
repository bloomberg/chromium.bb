// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/profile_management_switches.h"

#include <memory>

#include "components/signin/core/browser/scoped_unified_consent.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

TEST(ProfileManagementSwitchesTest, UnifiedConsent) {
  // Unified consent is disabled by default.
  EXPECT_EQ(UnifiedConsentFeatureState::kDisabled,
            GetUnifiedConsentFeatureState());

  for (UnifiedConsentFeatureState state :
       {UnifiedConsentFeatureState::kDisabled,
        UnifiedConsentFeatureState::kEnabledNoBump,
        UnifiedConsentFeatureState::kEnabledWithBump}) {
    ScopedUnifiedConsent scoped_state(state);
    EXPECT_EQ(state, GetUnifiedConsentFeatureState());
  }
}

}  // namespace signin
