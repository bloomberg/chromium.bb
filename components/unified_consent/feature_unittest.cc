// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/feature.h"

#include "build/build_config.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/unified_consent/scoped_unified_consent.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace unified_consent {

TEST(UnifiedConsentFeatureTest, FeatureState) {
// Linux platforms
#if defined(OS_ANDROID)
  // Unified consent is disabled by default.
  EXPECT_TRUE(IsUnifiedConsentFeatureEnabled());
#endif

#if defined(OS_CHROMEOS)
  // Unified consent is disabled by default.
  EXPECT_FALSE(IsUnifiedConsentFeatureEnabled());
#endif

#if defined(OS_LINUX) && !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  // Unified consent is enabled by default.
  EXPECT_TRUE(IsUnifiedConsentFeatureEnabled());
#endif

// Apple platforms
#if defined(OS_IOS)
  // Unified consent is enabled by default.
  EXPECT_TRUE(IsUnifiedConsentFeatureEnabled());
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Unified consent is enabled by default.
  EXPECT_TRUE(IsUnifiedConsentFeatureEnabled());
#endif

// Windows
#if defined(OS_WIN)
  // Unified consent is enabled by default.
  EXPECT_TRUE(IsUnifiedConsentFeatureEnabled());
#endif

  {
    ScopedUnifiedConsent scoped_disabled(UnifiedConsentFeatureState::kDisabled);
    EXPECT_FALSE(IsUnifiedConsentFeatureEnabled());
  }

  {
    ScopedUnifiedConsent scoped_enabled(UnifiedConsentFeatureState::kEnabled);
    EXPECT_TRUE(IsUnifiedConsentFeatureEnabled());
  }
}

}  // namespace unified_consent
