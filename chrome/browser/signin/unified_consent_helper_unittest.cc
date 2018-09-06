// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/unified_consent_helper.h"

#include "chrome/test/base/testing_profile.h"
#include "components/unified_consent/feature.h"
#include "components/unified_consent/scoped_unified_consent.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

// Checks that the feature state for the profile is the same as the global
// feature state.
TEST(UnifiedConsentHelperTest, FeatureState) {
  content::TestBrowserThreadBundle thread_bundle;
  TestingProfile profile;

  // Unified consent is disabled by default.
  EXPECT_FALSE(IsUnifiedConsentFeatureEnabled(&profile));
  EXPECT_FALSE(IsUnifiedConsentFeatureWithBumpEnabled(&profile));

  // The feature state for the profile is the same as the global feature state.
  {
    unified_consent::ScopedUnifiedConsent scoped_disabled(
        unified_consent::UnifiedConsentFeatureState::kDisabled);
    EXPECT_FALSE(IsUnifiedConsentFeatureEnabled(&profile));
    EXPECT_FALSE(IsUnifiedConsentFeatureWithBumpEnabled(&profile));
  }

  {
    unified_consent::ScopedUnifiedConsent scoped_no_bump(
        unified_consent::UnifiedConsentFeatureState::kEnabledNoBump);
    EXPECT_TRUE(IsUnifiedConsentFeatureEnabled(&profile));
    EXPECT_FALSE(IsUnifiedConsentFeatureWithBumpEnabled(&profile));
  }

  {
    unified_consent::ScopedUnifiedConsent scoped_bump(
        unified_consent::UnifiedConsentFeatureState::kEnabledWithBump);
    EXPECT_TRUE(IsUnifiedConsentFeatureEnabled(&profile));
    EXPECT_TRUE(IsUnifiedConsentFeatureWithBumpEnabled(&profile));
  }
}
