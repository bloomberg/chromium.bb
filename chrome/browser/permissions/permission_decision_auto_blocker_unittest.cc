// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_decision_auto_blocker.h"

#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_browsing_db/test_database_manager.h"
#include "content/public/browser/permission_type.h"

namespace {

bool FilterGoogle(const GURL& url) {
  return url == "https://www.google.com/";
}

bool FilterAll(const GURL& url) {
  return true;
}

void AutoBlockerCallback(bool expected, bool result) {
  EXPECT_EQ(expected, result);
}

}  // namespace

// TODO(meredithl): Write unit tests to simulate entering Permissions
// Blacklisting embargo status via the public API.
class PermissionDecisionAutoBlockerUnitTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  int GetDismissalCount(const GURL& url, content::PermissionType permission) {
    return PermissionDecisionAutoBlocker::GetDismissCount(url, permission,
                                                          profile());
  }

  int GetIgnoreCount(const GURL& url, content::PermissionType permission) {
    return PermissionDecisionAutoBlocker::GetIgnoreCount(url, permission,
                                                         profile());
  }

  int RecordDismissAndEmbargo(const GURL& url,
                              content::PermissionType permission,
                              base::Time current_time) {
    return PermissionDecisionAutoBlocker::RecordDismissAndEmbargo(
        url, permission, profile(), current_time);
  }

  int RecordIgnore(const GURL& url, content::PermissionType permission) {
    return PermissionDecisionAutoBlocker::RecordIgnore(url, permission,
                                                       profile());
  }

  void UpdateEmbargoedStatus(content::PermissionType permission,
                             const GURL& url,
                             base::Time current_time,
                             bool expected_result) {
    PermissionDecisionAutoBlocker::UpdateEmbargoedStatus(
        nullptr /* db manager */, permission, url, nullptr /* web contents */,
        2000 /* timeout in ms */, profile(), current_time,
        base::Bind(&AutoBlockerCallback, expected_result));
  }

  // Manually placing an origin, permission pair under embargo for blacklisting.
  // To embargo on dismissals, RecordDismissAndEmbargo can be used.
  void PlaceUnderBlacklistEmbargo(content::PermissionType permission,
                                  const GURL& url,
                                  HostContentSettingsMap* map,
                                  base::Time current_time) {
    PermissionDecisionAutoBlocker::PlaceUnderEmbargo(
        permission, url, map, current_time,
        PermissionDecisionAutoBlocker::kPermissionBlacklistEmbargoKey);
  }
};

TEST_F(PermissionDecisionAutoBlockerUnitTest, RemoveCountsByUrl) {
  GURL url1("https://www.google.com");
  GURL url2("https://www.example.com");
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kBlockPromptsIfDismissedOften);

  // Record some dismissals.
  EXPECT_FALSE(RecordDismissAndEmbargo(
      url1, content::PermissionType::GEOLOCATION, base::Time::Now()));
  EXPECT_EQ(1, GetDismissalCount(url1, content::PermissionType::GEOLOCATION));

  EXPECT_FALSE(RecordDismissAndEmbargo(
      url1, content::PermissionType::GEOLOCATION, base::Time::Now()));
  EXPECT_EQ(2, GetDismissalCount(url1, content::PermissionType::GEOLOCATION));

  EXPECT_TRUE(RecordDismissAndEmbargo(
      url1, content::PermissionType::GEOLOCATION, base::Time::Now()));
  EXPECT_EQ(3, GetDismissalCount(url1, content::PermissionType::GEOLOCATION));

  EXPECT_FALSE(RecordDismissAndEmbargo(
      url2, content::PermissionType::GEOLOCATION, base::Time::Now()));
  EXPECT_EQ(1, GetDismissalCount(url2, content::PermissionType::GEOLOCATION));

  EXPECT_FALSE(RecordDismissAndEmbargo(
      url1, content::PermissionType::NOTIFICATIONS, base::Time::Now()));
  EXPECT_EQ(1, GetDismissalCount(url1, content::PermissionType::NOTIFICATIONS));

  // Record some ignores.
  EXPECT_EQ(1, RecordIgnore(url1, content::PermissionType::MIDI_SYSEX));
  EXPECT_EQ(1, RecordIgnore(url1, content::PermissionType::DURABLE_STORAGE));
  EXPECT_EQ(1, RecordIgnore(url2, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(2, RecordIgnore(url2, content::PermissionType::GEOLOCATION));

  PermissionDecisionAutoBlocker::RemoveCountsByUrl(profile(),
                                                   base::Bind(&FilterGoogle));

  // Expect that url1's actions are gone, but url2's remain.
  EXPECT_EQ(0, GetDismissalCount(url1, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(0, GetDismissalCount(url1, content::PermissionType::NOTIFICATIONS));
  EXPECT_EQ(0, GetIgnoreCount(url1, content::PermissionType::MIDI_SYSEX));
  EXPECT_EQ(0, GetIgnoreCount(url1, content::PermissionType::DURABLE_STORAGE));

  EXPECT_EQ(1, GetDismissalCount(url2, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(2, GetIgnoreCount(url2, content::PermissionType::GEOLOCATION));

  // Add some more actions.
  EXPECT_FALSE(RecordDismissAndEmbargo(
      url1, content::PermissionType::GEOLOCATION, base::Time::Now()));
  EXPECT_EQ(1, GetDismissalCount(url1, content::PermissionType::GEOLOCATION));

  EXPECT_FALSE(RecordDismissAndEmbargo(
      url1, content::PermissionType::NOTIFICATIONS, base::Time::Now()));
  EXPECT_EQ(1, GetDismissalCount(url1, content::PermissionType::NOTIFICATIONS));

  EXPECT_FALSE(RecordDismissAndEmbargo(
      url2, content::PermissionType::GEOLOCATION, base::Time::Now()));
  EXPECT_EQ(2, GetDismissalCount(url2, content::PermissionType::GEOLOCATION));

  EXPECT_EQ(1, RecordIgnore(url1, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(1, RecordIgnore(url1, content::PermissionType::NOTIFICATIONS));
  EXPECT_EQ(1, RecordIgnore(url1, content::PermissionType::DURABLE_STORAGE));
  EXPECT_EQ(1, RecordIgnore(url2, content::PermissionType::MIDI_SYSEX));

  // Remove everything and expect that it's all gone.
  PermissionDecisionAutoBlocker::RemoveCountsByUrl(profile(),
                                                   base::Bind(&FilterAll));

  EXPECT_EQ(0, GetDismissalCount(url1, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(0, GetDismissalCount(url1, content::PermissionType::NOTIFICATIONS));
  EXPECT_EQ(0, GetDismissalCount(url2, content::PermissionType::GEOLOCATION));

  EXPECT_EQ(0, GetIgnoreCount(url1, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(0, GetIgnoreCount(url1, content::PermissionType::NOTIFICATIONS));
  EXPECT_EQ(0, GetIgnoreCount(url2, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(0, GetIgnoreCount(url2, content::PermissionType::DURABLE_STORAGE));
  EXPECT_EQ(0, GetIgnoreCount(url2, content::PermissionType::MIDI_SYSEX));
}

// Check that IsUnderEmbargo returns the correct value when the embargo is set
// and expires.
TEST_F(PermissionDecisionAutoBlockerUnitTest, CheckEmbargoStatus) {
  GURL url("https://www.google.com");
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  base::Time time_now = base::Time::Now();
  base::test::ScopedFeatureList feature_list;

  feature_list.InitAndEnableFeature(features::kPermissionsBlacklist);
  EXPECT_TRUE(base::FeatureList::IsEnabled(features::kPermissionsBlacklist));

  // Manually place url under embargo and confirm embargo status.
  PlaceUnderBlacklistEmbargo(content::PermissionType::GEOLOCATION, url, map,
                             time_now);
  EXPECT_TRUE(PermissionDecisionAutoBlocker::IsUnderEmbargo(
      content::PermissionType::GEOLOCATION, profile(), url, time_now));

  // Check that the origin is not under embargo for another permission.
  EXPECT_FALSE(PermissionDecisionAutoBlocker::IsUnderEmbargo(
      content::PermissionType::NOTIFICATIONS, profile(), url, time_now));

  // Confirm embargo status during the embargo period.
  EXPECT_TRUE(PermissionDecisionAutoBlocker::IsUnderEmbargo(
      content::PermissionType::GEOLOCATION, profile(), url,
      time_now + base::TimeDelta::FromDays(5)));

  // Check embargo is lifted on expiry day. A small offset after the exact
  // embargo expiration date has been added to account for any precision errors
  // when removing the date stored as a double from the permission dictionary.
  EXPECT_FALSE(PermissionDecisionAutoBlocker::IsUnderEmbargo(
      content::PermissionType::GEOLOCATION, profile(), url,
      time_now + base::TimeDelta::FromHours(7 * 24 + 1)));

  // Check embargo is lifted well after the expiry day.
  EXPECT_FALSE(PermissionDecisionAutoBlocker::IsUnderEmbargo(
      content::PermissionType::GEOLOCATION, profile(), url,
      time_now + base::TimeDelta::FromDays(8)));

  // Place under embargo again and verify the embargo status.
  time_now = base::Time::Now();
  PlaceUnderBlacklistEmbargo(content::PermissionType::NOTIFICATIONS, url, map,
                             time_now);
  EXPECT_TRUE(PermissionDecisionAutoBlocker::IsUnderEmbargo(
      content::PermissionType::NOTIFICATIONS, profile(), url, time_now));
}

// Tests the alternating pattern of the block on multiple dismiss behaviour. On
// N dismissals, the origin to be embargoed for the requested permission and
// automatically blocked. Each time the embargo is lifted, the site gets another
// chance to request the permission, but if it is again dismissed it is placed
// under embargo again and its permission requests blocked.
TEST_F(PermissionDecisionAutoBlockerUnitTest, TestDismissEmbargo) {
  GURL url("https://www.google.com");
  base::Time time_now = base::Time::Now();
  // Enable the autoblocking feature, which is disabled by default.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kBlockPromptsIfDismissedOften);

  EXPECT_TRUE(
      base::FeatureList::IsEnabled(features::kBlockPromptsIfDismissedOften));

  // Record some dismisses.
  EXPECT_FALSE(RecordDismissAndEmbargo(
      url, content::PermissionType::GEOLOCATION, time_now));
  EXPECT_FALSE(RecordDismissAndEmbargo(
      url, content::PermissionType::GEOLOCATION, time_now));

  // A request with < 3 prior dismisses should not be automatically blocked.
  EXPECT_FALSE(PermissionDecisionAutoBlocker::IsUnderEmbargo(
      content::PermissionType::GEOLOCATION, profile(), url, time_now));

  // After the 3rd dismiss subsequent permission requests should be autoblocked.
  EXPECT_TRUE(RecordDismissAndEmbargo(url, content::PermissionType::GEOLOCATION,
                                      time_now));
  EXPECT_TRUE(PermissionDecisionAutoBlocker::IsUnderEmbargo(
      content::PermissionType::GEOLOCATION, profile(), url, time_now));

  // Accelerate time forward, check that the embargo status is lifted and the
  // request won't be automatically blocked.
  time_now += base::TimeDelta::FromDays(8);
  EXPECT_FALSE(PermissionDecisionAutoBlocker::IsUnderEmbargo(
      content::PermissionType::GEOLOCATION, profile(), url, time_now));

  // Record another dismiss, subsequent requests should be autoblocked again.
  EXPECT_TRUE(RecordDismissAndEmbargo(url, content::PermissionType::GEOLOCATION,
                                      time_now));
  EXPECT_TRUE(PermissionDecisionAutoBlocker::IsUnderEmbargo(
      content::PermissionType::GEOLOCATION, profile(), url, time_now));

  // Accelerate time again, check embargo is lifted and another permission
  // request is let through.
  time_now += base::TimeDelta::FromDays(8);
  EXPECT_FALSE(PermissionDecisionAutoBlocker::IsUnderEmbargo(
      content::PermissionType::GEOLOCATION, profile(), url, time_now));
}

// Test the logic for a combination of blacklisting and dismissal embargo.
TEST_F(PermissionDecisionAutoBlockerUnitTest, TestExpiredBlacklistEmbargo) {
  GURL url("https://www.google.com");
  base::Time time_now = base::Time::Now();
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());

  // Enable both dismissals and permissions blacklisting features.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kBlockPromptsIfDismissedOften,
                                 features::kPermissionsBlacklist},
                                {});
  EXPECT_TRUE(
      base::FeatureList::IsEnabled(features::kBlockPromptsIfDismissedOften));
  EXPECT_TRUE(base::FeatureList::IsEnabled(features::kPermissionsBlacklist));

  // Place under blacklist embargo and check the status.
  PlaceUnderBlacklistEmbargo(content::PermissionType::GEOLOCATION, url, map,
                             base::Time::Now());

  time_now += base::TimeDelta::FromDays(5);
  EXPECT_TRUE(PermissionDecisionAutoBlocker::IsUnderEmbargo(
      content::PermissionType::GEOLOCATION, profile(), url, time_now));

  // Record dismisses to place it under dismissal embargo.
  EXPECT_FALSE(RecordDismissAndEmbargo(
      url, content::PermissionType::GEOLOCATION, time_now));
  EXPECT_FALSE(RecordDismissAndEmbargo(
      url, content::PermissionType::GEOLOCATION, time_now));
  EXPECT_TRUE(RecordDismissAndEmbargo(url, content::PermissionType::GEOLOCATION,
                                      time_now));

  // Accelerate time to a point where the blacklist embargo should be expired.
  time_now += base::TimeDelta::FromDays(3);
  EXPECT_FALSE(PermissionDecisionAutoBlocker::IsUnderEmbargo(
      content::PermissionType::GEOLOCATION, profile(), url,
      time_now + base::TimeDelta::FromDays(5)));

  // Check that dismissal embargo is still set, even though the blacklisting
  // embargo has expired.
  EXPECT_TRUE(PermissionDecisionAutoBlocker::IsUnderEmbargo(
      content::PermissionType::GEOLOCATION, profile(), url, time_now));
}
