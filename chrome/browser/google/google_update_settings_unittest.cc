// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google/google_util.h"
#include "chrome/installer/util/google_update_settings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class GoogleUpdateTest : public PlatformTest {
};

TEST_F(GoogleUpdateTest, StatsConsent) {
  // Stats are off by default.
  EXPECT_FALSE(GoogleUpdateSettings::GetCollectStatsConsent());
  // Stats reporting is ON.
  EXPECT_TRUE(GoogleUpdateSettings::SetCollectStatsConsent(true));
  EXPECT_TRUE(GoogleUpdateSettings::GetCollectStatsConsent());
  // Stats reporting is OFF.
  EXPECT_TRUE(GoogleUpdateSettings::SetCollectStatsConsent(false));
  EXPECT_FALSE(GoogleUpdateSettings::GetCollectStatsConsent());
}

#if defined(OS_WIN)

TEST_F(GoogleUpdateTest, LastRunTime) {
  // Querying the value that does not exists should fail.
  EXPECT_TRUE(GoogleUpdateSettings::RemoveLastRunTime());
  EXPECT_EQ(-1, GoogleUpdateSettings::GetLastRunTime());
  // Setting and querying the last update time in fast sequence
  // should give 0 days.
  EXPECT_TRUE(GoogleUpdateSettings::SetLastRunTime());
  EXPECT_EQ(0, GoogleUpdateSettings::GetLastRunTime());
}

#endif  // defined(OS_WIN)

TEST_F(GoogleUpdateTest, IsOrganicFirstRunBrandCodes) {
  // Test some brand codes to ensure that future changes to this method won't
  // go unnoticed.
  EXPECT_FALSE(google_util::IsOrganicFirstRun("CHFO"));
  EXPECT_FALSE(google_util::IsOrganicFirstRun("CHMA"));
  EXPECT_TRUE(google_util::IsOrganicFirstRun("EUBA"));
  EXPECT_TRUE(google_util::IsOrganicFirstRun("GGRA"));

#if defined(OS_MACOSX)
  // An empty brand string on Mac is used for channels other than stable,
  // which are always organic.
  EXPECT_TRUE(google_util::IsOrganicFirstRun(""));
#endif
}
