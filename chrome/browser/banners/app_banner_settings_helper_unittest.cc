// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"

namespace {

const char kTestURL[] = "http://www.google.com";
const char kTestPackageName[] = "test.package";

bool IsWithinDay(base::Time time1, base::Time time2) {
  return time1 - time2 < base::TimeDelta::FromDays(1) ||
         time2 - time1 < base::TimeDelta::FromDays(1);
}

class AppBannerSettingsHelperTest : public ChromeRenderViewHostTestHarness {};

}  // namespace

TEST_F(AppBannerSettingsHelperTest, Block) {
  GURL url(kTestURL);
  NavigateAndCommit(url);

  // Check that by default, showing the banner is allowed.
  EXPECT_TRUE(AppBannerSettingsHelper::IsAllowed(web_contents(), url,
                                                 kTestPackageName));

  // Block the banner and test it is no longer allowed.
  AppBannerSettingsHelper::Block(web_contents(), url, kTestPackageName);
  EXPECT_FALSE(AppBannerSettingsHelper::IsAllowed(web_contents(), url,
                                                  kTestPackageName));
}

TEST_F(AppBannerSettingsHelperTest, CouldShowEvents) {
  GURL url(kTestURL);
  NavigateAndCommit(url);

  // Check that by default, there are no events recorded.
  std::vector<base::Time> events =
      AppBannerSettingsHelper::GetCouldShowBannerEvents(web_contents(), url,
                                                        kTestPackageName);
  EXPECT_TRUE(events.empty());

  base::Time::Exploded exploded_reference_time;
  exploded_reference_time.year = 2015;
  exploded_reference_time.month = 1;
  exploded_reference_time.day_of_month = 30;
  exploded_reference_time.day_of_week = 5;
  exploded_reference_time.hour = 11;
  exploded_reference_time.minute = 0;
  exploded_reference_time.second = 0;
  exploded_reference_time.millisecond = 0;

  base::Time reference_time =
      base::Time::FromLocalExploded(exploded_reference_time);
  base::Time same_day = reference_time + base::TimeDelta::FromHours(2);
  base::Time three_days_prior = reference_time - base::TimeDelta::FromDays(3);
  base::Time previous_fortnight =
      reference_time - base::TimeDelta::FromDays(14);

  // Test adding the first date.
  AppBannerSettingsHelper::RecordCouldShowBannerEvent(
      web_contents(), url, kTestPackageName, previous_fortnight);

  // It should be the only date recorded.
  events = AppBannerSettingsHelper::GetCouldShowBannerEvents(
      web_contents(), url, kTestPackageName);
  EXPECT_EQ(1u, events.size());
  EXPECT_TRUE(IsWithinDay(events[0], previous_fortnight));

  // Now add the next date.
  AppBannerSettingsHelper::RecordCouldShowBannerEvent(
      web_contents(), url, kTestPackageName, three_days_prior);

  // Now there should be two days.
  events = AppBannerSettingsHelper::GetCouldShowBannerEvents(
      web_contents(), url, kTestPackageName);
  EXPECT_EQ(2u, events.size());
  EXPECT_TRUE(IsWithinDay(events[0], previous_fortnight));
  EXPECT_TRUE(IsWithinDay(events[1], three_days_prior));

  // Now add the reference date.
  AppBannerSettingsHelper::RecordCouldShowBannerEvent(
      web_contents(), url, kTestPackageName, reference_time);

  // Now there should still be two days, but the first date should have been
  // removed.
  events = AppBannerSettingsHelper::GetCouldShowBannerEvents(
      web_contents(), url, kTestPackageName);
  EXPECT_EQ(2u, events.size());
  EXPECT_TRUE(IsWithinDay(events[0], three_days_prior));
  EXPECT_TRUE(IsWithinDay(events[1], reference_time));

  // Now add the the other day on the reference date.
  AppBannerSettingsHelper::RecordCouldShowBannerEvent(
      web_contents(), url, kTestPackageName, same_day);

  // Now there should still be the same two days.
  events = AppBannerSettingsHelper::GetCouldShowBannerEvents(
      web_contents(), url, kTestPackageName);
  EXPECT_EQ(2u, events.size());
  EXPECT_TRUE(IsWithinDay(events[0], three_days_prior));
  EXPECT_TRUE(IsWithinDay(events[1], reference_time));
}
