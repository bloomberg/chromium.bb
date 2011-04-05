// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

TEST_F(GoogleUpdateTest, ShouldShowSearchEngineDialog) {
  // Test some brand codes to ensure that future changes to this method won't
  // go unnoticed.
  const wchar_t* false_brand1 = L"CHFO";
  EXPECT_FALSE(GoogleUpdateSettings::IsOrganicFirstRun(
      false_brand1));
  const wchar_t* false_brand2 = L"CHMA";
  EXPECT_FALSE(GoogleUpdateSettings::IsOrganicFirstRun(
      false_brand2));
  const wchar_t* good_brand1 = L"EUBA";
  EXPECT_TRUE(GoogleUpdateSettings::IsOrganicFirstRun(
      good_brand1));
  const wchar_t* good_brand2 = L"GGRA";
  EXPECT_TRUE(GoogleUpdateSettings::IsOrganicFirstRun(
      good_brand2));
}

#endif  // defined(OS_WIN)
