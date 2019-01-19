// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/test_scoped_offline_clock.h"
#include "components/prefs/testing_pref_service.h"
#include "components/version_info/version_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class PrefetchPrefsTest : public testing::Test {
 public:
  void SetUp() override { prefetch_prefs::RegisterPrefs(prefs()->registry()); }

  TestingPrefServiceSimple* prefs() { return &pref_service_; }

 private:
  TestingPrefServiceSimple pref_service_;
};

TEST_F(PrefetchPrefsTest, PrefetchingEnabled) {
  bool is_enabled_by_default = IsPrefetchingOfflinePagesEnabled();

  EXPECT_EQ(is_enabled_by_default, prefetch_prefs::IsEnabled(prefs()));

  // If disabled by default, should remain disabled.
  prefetch_prefs::SetPrefetchingEnabledInSettings(prefs(), true);
  EXPECT_EQ(is_enabled_by_default, prefetch_prefs::IsEnabled(prefs()));

  prefetch_prefs::SetPrefetchingEnabledInSettings(prefs(), false);
  EXPECT_FALSE(prefetch_prefs::IsEnabled(prefs()));
}

TEST_F(PrefetchPrefsTest, LimitlessPrefetchingEnabled) {
  // Check that the default value is false.
  EXPECT_FALSE(prefetch_prefs::IsLimitlessPrefetchingEnabled(prefs()));

  // Check that limitless can be enabled.
  prefetch_prefs::SetLimitlessPrefetchingEnabled(prefs(), true);
  EXPECT_TRUE(prefetch_prefs::IsLimitlessPrefetchingEnabled(prefs()));

  // Check that it can be disabled.
  prefetch_prefs::SetLimitlessPrefetchingEnabled(prefs(), false);
  EXPECT_FALSE(prefetch_prefs::IsLimitlessPrefetchingEnabled(prefs()));

  // Simulate time passing to check that the setting turns itself off as
  // expected.
  base::TimeDelta enabled_duration;
  if (version_info::IsOfficialBuild())
    enabled_duration = base::TimeDelta::FromDays(1);
  else
    enabled_duration = base::TimeDelta::FromDays(365);

  base::TimeDelta advance_delta = base::TimeDelta::FromHours(2);
  base::Time now = OfflineTimeNow();

  prefetch_prefs::SetLimitlessPrefetchingEnabled(prefs(), true);
  TestScopedOfflineClock test_clock;

  // Set time to just before the setting expires:
  test_clock.SetNow(now + enabled_duration - advance_delta);
  EXPECT_TRUE(prefetch_prefs::IsLimitlessPrefetchingEnabled(prefs()));

  // Advance to just after it expires:
  test_clock.Advance(2 * advance_delta);
  EXPECT_FALSE(prefetch_prefs::IsLimitlessPrefetchingEnabled(prefs()));
}

}  // namespace offline_pages
