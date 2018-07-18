// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/unified_consent_helper.h"

#include "build/buildflag.h"
#include "chrome/browser/signin/scoped_account_consistency.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/signin_buildflags.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/unified_consent/feature.h"
#include "components/unified_consent/scoped_unified_consent.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

// On Dice platforms, unified consent can only be enabled for Dice profiles.
TEST(UnifiedConsentHelperTest, DiceDisabled) {
  // Disable Dice.
  ScopedAccountConsistencyDiceFixAuthErrors dice_fix_auth_errors;

  content::TestBrowserThreadBundle thread_bundle;
  TestingProfile profile;

  for (unified_consent::UnifiedConsentFeatureState state :
       {unified_consent::UnifiedConsentFeatureState::kDisabled,
        unified_consent::UnifiedConsentFeatureState::kEnabledNoBump,
        unified_consent::UnifiedConsentFeatureState::kEnabledWithBump}) {
    unified_consent::ScopedUnifiedConsent scoped_state(state);
    EXPECT_FALSE(IsUnifiedConsentEnabled(&profile));
    EXPECT_FALSE(IsUnifiedConsentBumpEnabled(&profile));
  }
}

#endif

// Checks that the feature state for the profile is the same as the global
// feature state.
TEST(UnifiedConsentHelperTest, FeatureState) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Enable Dice.
  ScopedAccountConsistencyDice dice;
#endif

  content::TestBrowserThreadBundle thread_bundle;
  TestingProfile profile;

  // Unified consent is disabled by default.
  EXPECT_FALSE(IsUnifiedConsentEnabled(&profile));
  EXPECT_FALSE(IsUnifiedConsentBumpEnabled(&profile));

  // The feature state for the profile is the same as the global feature state.
  {
    unified_consent::ScopedUnifiedConsent scoped_disabled(
        unified_consent::UnifiedConsentFeatureState::kDisabled);
    EXPECT_FALSE(IsUnifiedConsentEnabled(&profile));
    EXPECT_FALSE(IsUnifiedConsentBumpEnabled(&profile));
  }

  {
    unified_consent::ScopedUnifiedConsent scoped_no_bump(
        unified_consent::UnifiedConsentFeatureState::kEnabledNoBump);
    EXPECT_TRUE(IsUnifiedConsentEnabled(&profile));
    EXPECT_FALSE(IsUnifiedConsentBumpEnabled(&profile));
  }

  {
    unified_consent::ScopedUnifiedConsent scoped_bump(
        unified_consent::UnifiedConsentFeatureState::kEnabledWithBump);
    EXPECT_TRUE(IsUnifiedConsentEnabled(&profile));
    EXPECT_TRUE(IsUnifiedConsentBumpEnabled(&profile));
  }
}
