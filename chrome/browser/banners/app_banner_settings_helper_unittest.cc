// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"

namespace {

const char kTestURL[] = "https://www.google.com";
const char kTestPackageName[] = "test.package";

base::Time GetReferenceTime() {
  base::Time::Exploded exploded_reference_time;
  exploded_reference_time.year = 2015;
  exploded_reference_time.month = 1;
  exploded_reference_time.day_of_month = 30;
  exploded_reference_time.day_of_week = 5;
  exploded_reference_time.hour = 11;
  exploded_reference_time.minute = 0;
  exploded_reference_time.second = 0;
  exploded_reference_time.millisecond = 0;

  return base::Time::FromLocalExploded(exploded_reference_time);
}

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

  base::Time reference_time = GetReferenceTime();
  base::Time same_day = reference_time + base::TimeDelta::FromHours(2);
  base::Time three_days_prior = reference_time - base::TimeDelta::FromDays(3);
  base::Time previous_fortnight =
      reference_time - base::TimeDelta::FromDays(14);

  // Test adding the first date.
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_COULD_SHOW, previous_fortnight);

  // It should be the only date recorded.
  events = AppBannerSettingsHelper::GetCouldShowBannerEvents(
      web_contents(), url, kTestPackageName);
  EXPECT_EQ(1u, events.size());
  EXPECT_TRUE(IsWithinDay(events[0], previous_fortnight));

  // Now add the next date.
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_COULD_SHOW, three_days_prior);

  // Now there should be two days.
  events = AppBannerSettingsHelper::GetCouldShowBannerEvents(
      web_contents(), url, kTestPackageName);
  EXPECT_EQ(2u, events.size());
  EXPECT_TRUE(IsWithinDay(events[0], previous_fortnight));
  EXPECT_TRUE(IsWithinDay(events[1], three_days_prior));

  // Now add the reference date.
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_COULD_SHOW, reference_time);

  // Now there should still be two days, but the first date should have been
  // removed.
  events = AppBannerSettingsHelper::GetCouldShowBannerEvents(
      web_contents(), url, kTestPackageName);
  EXPECT_EQ(2u, events.size());
  EXPECT_TRUE(IsWithinDay(events[0], three_days_prior));
  EXPECT_TRUE(IsWithinDay(events[1], reference_time));

  // Now add the the other day on the reference date.
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_COULD_SHOW, same_day);

  // Now there should still be the same two days.
  events = AppBannerSettingsHelper::GetCouldShowBannerEvents(
      web_contents(), url, kTestPackageName);
  EXPECT_EQ(2u, events.size());
  EXPECT_TRUE(IsWithinDay(events[0], three_days_prior));
  EXPECT_TRUE(IsWithinDay(events[1], reference_time));
}

TEST_F(AppBannerSettingsHelperTest, SingleEvents) {
  GURL url(kTestURL);
  NavigateAndCommit(url);

  base::Time reference_time = GetReferenceTime();
  base::Time other_time = reference_time - base::TimeDelta::FromDays(3);
  for (int event = AppBannerSettingsHelper::APP_BANNER_EVENT_DID_SHOW;
       event < AppBannerSettingsHelper::APP_BANNER_EVENT_NUM_EVENTS; ++event) {
    // Check that by default, there is no event.
    base::Time event_time = AppBannerSettingsHelper::GetSingleBannerEvent(
        web_contents(), url, kTestPackageName,
        AppBannerSettingsHelper::AppBannerEvent(event));
    EXPECT_TRUE(event_time.is_null());

    // Check that a time can be recorded.
    AppBannerSettingsHelper::RecordBannerEvent(
        web_contents(), url, kTestPackageName,
        AppBannerSettingsHelper::AppBannerEvent(event), reference_time);

    event_time = AppBannerSettingsHelper::GetSingleBannerEvent(
        web_contents(), url, kTestPackageName,
        AppBannerSettingsHelper::AppBannerEvent(event));
    EXPECT_EQ(reference_time, event_time);

    // Check that another time can be recorded.
    AppBannerSettingsHelper::RecordBannerEvent(
        web_contents(), url, kTestPackageName,
        AppBannerSettingsHelper::AppBannerEvent(event), other_time);

    event_time = AppBannerSettingsHelper::GetSingleBannerEvent(
        web_contents(), url, kTestPackageName,
        AppBannerSettingsHelper::AppBannerEvent(event));
    EXPECT_EQ(other_time, event_time);
  }
}

TEST_F(AppBannerSettingsHelperTest, ShouldShowFromEngagement) {
  GURL url(kTestURL);
  NavigateAndCommit(url);

  base::Time reference_time = GetReferenceTime();
  base::Time one_day_ago = reference_time - base::TimeDelta::FromDays(1);
  base::Time one_year_ago = reference_time - base::TimeDelta::FromDays(366);

  // By default the banner should not be shown.
  EXPECT_FALSE(AppBannerSettingsHelper::ShouldShowBanner(
      web_contents(), url, kTestPackageName, reference_time));

  // Visit the site once, it still should not be shown.
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_COULD_SHOW, one_year_ago);
  EXPECT_FALSE(AppBannerSettingsHelper::ShouldShowBanner(
      web_contents(), url, kTestPackageName, reference_time));

  // Visit the site again after a long delay, it still should not be shown.
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_COULD_SHOW, one_day_ago);
  EXPECT_FALSE(AppBannerSettingsHelper::ShouldShowBanner(
      web_contents(), url, kTestPackageName, reference_time));

  // Visit the site again; now it should be shown.
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_COULD_SHOW, reference_time);
  EXPECT_TRUE(AppBannerSettingsHelper::ShouldShowBanner(
      web_contents(), url, kTestPackageName, reference_time));
}

TEST_F(AppBannerSettingsHelperTest, ShouldNotShowAfterBlocking) {
  GURL url(kTestURL);
  NavigateAndCommit(url);

  base::Time reference_time = GetReferenceTime();
  base::Time one_day_ago = reference_time - base::TimeDelta::FromDays(1);
  base::Time two_months_ago = reference_time - base::TimeDelta::FromDays(60);
  base::Time one_year_ago = reference_time - base::TimeDelta::FromDays(366);

  // By default the banner should not be shown.
  EXPECT_FALSE(AppBannerSettingsHelper::ShouldShowBanner(
      web_contents(), url, kTestPackageName, reference_time));

  // Record events such that the banner should show.
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_COULD_SHOW, one_day_ago);
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_COULD_SHOW, reference_time);
  EXPECT_TRUE(AppBannerSettingsHelper::ShouldShowBanner(
      web_contents(), url, kTestPackageName, reference_time));

  // Block the site a long time ago. It should still be shown.
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_BLOCK, one_year_ago);
  EXPECT_TRUE(AppBannerSettingsHelper::ShouldShowBanner(
      web_contents(), url, kTestPackageName, reference_time));

  // Block the site more recently. Now it should not be shown.
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_BLOCK, two_months_ago);
  EXPECT_FALSE(AppBannerSettingsHelper::ShouldShowBanner(
      web_contents(), url, kTestPackageName, reference_time));
}

TEST_F(AppBannerSettingsHelperTest, ShouldNotShowAfterShowing) {
  GURL url(kTestURL);
  NavigateAndCommit(url);

  base::Time reference_time = GetReferenceTime();
  base::Time one_day_ago = reference_time - base::TimeDelta::FromDays(1);
  base::Time three_weeks_ago = reference_time - base::TimeDelta::FromDays(21);
  base::Time one_year_ago = reference_time - base::TimeDelta::FromDays(366);

  // By default the banner should not be shown.
  EXPECT_FALSE(AppBannerSettingsHelper::ShouldShowBanner(
      web_contents(), url, kTestPackageName, reference_time));

  // Record events such that the banner should show.
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_COULD_SHOW, one_day_ago);
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_COULD_SHOW, reference_time);
  EXPECT_TRUE(AppBannerSettingsHelper::ShouldShowBanner(
      web_contents(), url, kTestPackageName, reference_time));

  // Show the banner a long time ago. It should still be shown.
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_SHOW, one_year_ago);
  EXPECT_TRUE(AppBannerSettingsHelper::ShouldShowBanner(
      web_contents(), url, kTestPackageName, reference_time));

  // Show the site more recently. Now it should not be shown.
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_SHOW, three_weeks_ago);
  EXPECT_FALSE(AppBannerSettingsHelper::ShouldShowBanner(
      web_contents(), url, kTestPackageName, reference_time));
}

TEST_F(AppBannerSettingsHelperTest, ShouldNotShowAfterAdding) {
  GURL url(kTestURL);
  NavigateAndCommit(url);

  base::Time reference_time = GetReferenceTime();
  base::Time one_day_ago = reference_time - base::TimeDelta::FromDays(1);
  base::Time one_year_ago = reference_time - base::TimeDelta::FromDays(366);

  // By default the banner should not be shown.
  EXPECT_FALSE(AppBannerSettingsHelper::ShouldShowBanner(
      web_contents(), url, kTestPackageName, reference_time));

  // Record events such that the banner should show.
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_COULD_SHOW, one_day_ago);
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_COULD_SHOW, reference_time);
  EXPECT_TRUE(AppBannerSettingsHelper::ShouldShowBanner(
      web_contents(), url, kTestPackageName, reference_time));

  // Add the site a long time ago. It should not be shown.
  AppBannerSettingsHelper::RecordBannerEvent(
      web_contents(), url, kTestPackageName,
      AppBannerSettingsHelper::APP_BANNER_EVENT_DID_ADD_TO_HOMESCREEN,
      one_year_ago);
  EXPECT_FALSE(AppBannerSettingsHelper::ShouldShowBanner(
      web_contents(), url, kTestPackageName, reference_time));
}
