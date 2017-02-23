// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_decision_auto_blocker.h"

#include <map>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_uma_util.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/safe_browsing_db/test_database_manager.h"

namespace {

bool FilterGoogle(const GURL& url) {
  return url == "https://www.google.com/";
}

bool FilterAll(const GURL& url) {
  return true;
}

class MockSafeBrowsingDatabaseManager
    : public safe_browsing::TestSafeBrowsingDatabaseManager {
 public:
  explicit MockSafeBrowsingDatabaseManager(bool perform_callback, bool enabled)
      : perform_callback_(perform_callback), enabled_(enabled) {}

  bool CheckApiBlacklistUrl(
      const GURL& url,
      safe_browsing::SafeBrowsingDatabaseManager::Client* client) override {
    // Return true when able to synchronously determine that the url is safe.
    if (!enabled_) {
      return true;
    }

    if (perform_callback_) {
      safe_browsing::ThreatMetadata metadata;
      const auto& blacklisted_permissions = permissions_blacklist_.find(url);
      if (blacklisted_permissions != permissions_blacklist_.end())
        metadata.api_permissions = blacklisted_permissions->second;
      client->OnCheckApiBlacklistUrlResult(url, metadata);
    }
    return false;
  }

  bool CancelApiCheck(Client* client) override {
    DCHECK(!perform_callback_);
    // Returns true when client check could be stopped.
    return true;
  }

  void BlacklistUrlPermissions(const GURL& url,
                               const std::set<std::string> permissions) {
    permissions_blacklist_[url] = permissions;
  }

  void SetPerformCallback(bool perform_callback) {
    perform_callback_ = perform_callback;
  }

 protected:
  ~MockSafeBrowsingDatabaseManager() override {}

 private:
  bool perform_callback_;
  bool enabled_;
  std::map<GURL, std::set<std::string>> permissions_blacklist_;

  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingDatabaseManager);
};

}  // namespace

class PermissionDecisionAutoBlockerUnitTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    autoblocker_ = PermissionDecisionAutoBlocker::GetForProfile(profile());
    feature_list_.InitWithFeatures({features::kBlockPromptsIfDismissedOften,
                                    features::kPermissionsBlacklist},
                                   {});
    last_embargoed_status_ = false;
    std::unique_ptr<base::SimpleTestClock> clock =
        base::MakeUnique<base::SimpleTestClock>();
    clock_ = clock.get();
    autoblocker_->SetClockForTesting(std::move(clock));
    callback_was_run_ = false;
  }

  void SetSafeBrowsingDatabaseManagerAndTimeoutForTesting(
      scoped_refptr<MockSafeBrowsingDatabaseManager> db_manager,
      int timeout) {
    autoblocker_->SetSafeBrowsingDatabaseManagerAndTimeoutForTesting(db_manager,
                                                                     timeout);
  }

  void CheckSafeBrowsingBlacklist(const GURL& url,
                                  ContentSettingsType permission) {
    base::RunLoop run_loop;
    autoblocker_->CheckSafeBrowsingBlacklist(
        nullptr, url, permission,
        base::Bind(&PermissionDecisionAutoBlockerUnitTest::SetLastEmbargoStatus,
                   base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Manually placing an (origin, permission) pair under embargo for
  // blacklisting. To embargo on dismissals, RecordDismissAndEmbargo can be
  // used.
  void PlaceUnderBlacklistEmbargo(const GURL& url,
                                  ContentSettingsType permission) {
    autoblocker_->PlaceUnderEmbargo(
        url, permission,
        PermissionDecisionAutoBlocker::kPermissionBlacklistEmbargoKey);
  }

  PermissionDecisionAutoBlocker* autoblocker() { return autoblocker_; }

  void SetLastEmbargoStatus(base::Closure quit_closure, bool status) {
    callback_was_run_ = true;
    last_embargoed_status_ = status;
    if (quit_closure) {
      quit_closure.Run();
      quit_closure.Reset();
    }
  }

  bool last_embargoed_status() { return last_embargoed_status_; }

  bool callback_was_run() { return callback_was_run_; }

  base::SimpleTestClock* clock() { return clock_; }

  const char* GetDismissKey() {
    return PermissionDecisionAutoBlocker::kPromptDismissCountKey;
  }

  const char* GetIgnoreKey() {
    return PermissionDecisionAutoBlocker::kPromptIgnoreCountKey;
  }

 private:
  PermissionDecisionAutoBlocker* autoblocker_;
  base::test::ScopedFeatureList feature_list_;
  base::SimpleTestClock* clock_;
  bool last_embargoed_status_;
  bool callback_was_run_;
};

TEST_F(PermissionDecisionAutoBlockerUnitTest, RemoveCountsByUrl) {
  GURL url1("https://www.google.com");
  GURL url2("https://www.example.com");

  // Record some dismissals.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_EQ(1, autoblocker()->GetDismissCount(
                   url1, CONTENT_SETTINGS_TYPE_GEOLOCATION));

  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_EQ(2, autoblocker()->GetDismissCount(
                   url1, CONTENT_SETTINGS_TYPE_GEOLOCATION));

  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url1, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_EQ(3, autoblocker()->GetDismissCount(
                   url1, CONTENT_SETTINGS_TYPE_GEOLOCATION));

  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url2, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_EQ(1, autoblocker()->GetDismissCount(
                   url2, CONTENT_SETTINGS_TYPE_GEOLOCATION));

  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
  EXPECT_EQ(1, autoblocker()->GetDismissCount(
                   url1, CONTENT_SETTINGS_TYPE_NOTIFICATIONS));

  // Record some ignores.
  EXPECT_EQ(
      1, autoblocker()->RecordIgnore(url1, CONTENT_SETTINGS_TYPE_MIDI_SYSEX));
  EXPECT_EQ(1, autoblocker()->RecordIgnore(
                   url1, CONTENT_SETTINGS_TYPE_DURABLE_STORAGE));
  EXPECT_EQ(
      1, autoblocker()->RecordIgnore(url2, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_EQ(
      2, autoblocker()->RecordIgnore(url2, CONTENT_SETTINGS_TYPE_GEOLOCATION));

  autoblocker()->RemoveCountsByUrl(base::Bind(&FilterGoogle));

  // Expect that url1's actions are gone, but url2's remain.
  EXPECT_EQ(0, autoblocker()->GetDismissCount(
                   url1, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_EQ(0, autoblocker()->GetDismissCount(
                   url1, CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
  EXPECT_EQ(
      0, autoblocker()->GetIgnoreCount(url1, CONTENT_SETTINGS_TYPE_MIDI_SYSEX));
  EXPECT_EQ(0, autoblocker()->GetIgnoreCount(
                   url1, CONTENT_SETTINGS_TYPE_DURABLE_STORAGE));

  EXPECT_EQ(1, autoblocker()->GetDismissCount(
                   url2, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_EQ(2, autoblocker()->GetIgnoreCount(
                   url2, CONTENT_SETTINGS_TYPE_GEOLOCATION));

  // Add some more actions.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_EQ(1, autoblocker()->GetDismissCount(
                   url1, CONTENT_SETTINGS_TYPE_GEOLOCATION));

  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
  EXPECT_EQ(1, autoblocker()->GetDismissCount(
                   url1, CONTENT_SETTINGS_TYPE_NOTIFICATIONS));

  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url2, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_EQ(2, autoblocker()->GetDismissCount(
                   url2, CONTENT_SETTINGS_TYPE_GEOLOCATION));

  EXPECT_EQ(
      1, autoblocker()->RecordIgnore(url1, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_EQ(1, autoblocker()->RecordIgnore(
                   url1, CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
  EXPECT_EQ(1, autoblocker()->RecordIgnore(
                   url1, CONTENT_SETTINGS_TYPE_DURABLE_STORAGE));
  EXPECT_EQ(
      1, autoblocker()->RecordIgnore(url2, CONTENT_SETTINGS_TYPE_MIDI_SYSEX));

  // Remove everything and expect that it's all gone.
  autoblocker()->RemoveCountsByUrl(base::Bind(&FilterAll));

  EXPECT_EQ(0, autoblocker()->GetDismissCount(
                   url1, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_EQ(0, autoblocker()->GetDismissCount(
                   url1, CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
  EXPECT_EQ(0, autoblocker()->GetDismissCount(
                   url2, CONTENT_SETTINGS_TYPE_GEOLOCATION));

  EXPECT_EQ(0, autoblocker()->GetIgnoreCount(
                   url1, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_EQ(0, autoblocker()->GetIgnoreCount(
                   url1, CONTENT_SETTINGS_TYPE_NOTIFICATIONS));
  EXPECT_EQ(0, autoblocker()->GetIgnoreCount(
                   url2, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_EQ(0, autoblocker()->GetIgnoreCount(
                   url2, CONTENT_SETTINGS_TYPE_DURABLE_STORAGE));
  EXPECT_EQ(
      0, autoblocker()->GetIgnoreCount(url2, CONTENT_SETTINGS_TYPE_MIDI_SYSEX));
}

// Test that an origin that has been blacklisted for a permission is embargoed.
TEST_F(PermissionDecisionAutoBlockerUnitTest, TestUpdateEmbargoBlacklist) {
  GURL url("https://www.google.com");
  base::HistogramTester histograms;

  scoped_refptr<MockSafeBrowsingDatabaseManager> db_manager =
      new MockSafeBrowsingDatabaseManager(true /* perform_callback */,
                                          true /* enabled */);
  std::set<std::string> blacklisted_permissions{"GEOLOCATION"};
  db_manager->BlacklistUrlPermissions(url, blacklisted_permissions);
  SetSafeBrowsingDatabaseManagerAndTimeoutForTesting(db_manager,
                                                     2000 /* timeout in ms */);

  CheckSafeBrowsingBlacklist(url, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  EXPECT_TRUE(callback_was_run());
  EXPECT_TRUE(last_embargoed_status());
  histograms.ExpectUniqueSample(
      "Permissions.AutoBlocker.SafeBrowsingResponse",
      static_cast<int>(SafeBrowsingResponse::BLACKLISTED), 1);
  histograms.ExpectTotalCount(
      "Permissions.AutoBlocker.SafeBrowsingResponseTime", 1);
}

// Test that an origin that is blacklisted for a permission will not be placed
// under embargoed for another.
TEST_F(PermissionDecisionAutoBlockerUnitTest, TestRequestNotBlacklisted) {
  GURL url("https://www.google.com");
  clock()->SetNow(base::Time::Now());
  base::HistogramTester histograms;

  scoped_refptr<MockSafeBrowsingDatabaseManager> db_manager =
      new MockSafeBrowsingDatabaseManager(true /* perform_callback */,
                                          true /* enabled */);
  std::set<std::string> blacklisted_permissions{"GEOLOCATION"};
  db_manager->BlacklistUrlPermissions(url, blacklisted_permissions);
  SetSafeBrowsingDatabaseManagerAndTimeoutForTesting(db_manager,
                                                     0 /* timeout in ms */);

  CheckSafeBrowsingBlacklist(url, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  EXPECT_FALSE(last_embargoed_status());
  histograms.ExpectUniqueSample(
      "Permissions.AutoBlocker.SafeBrowsingResponse",
      static_cast<int>(SafeBrowsingResponse::NOT_BLACKLISTED), 1);
  histograms.ExpectTotalCount(
      "Permissions.AutoBlocker.SafeBrowsingResponseTime", 1);
}

// Check that GetEmbargoResult returns the correct value when the embargo is set
// and expires.
TEST_F(PermissionDecisionAutoBlockerUnitTest, CheckEmbargoStatus) {
  GURL url("https://www.google.com");
  clock()->SetNow(base::Time::Now());

  // Check the default state.
  PermissionResult result =
      autoblocker()->GetEmbargoResult(url, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Place under embargo and verify.
  PlaceUnderBlacklistEmbargo(url, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  result =
      autoblocker()->GetEmbargoResult(url, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::SAFE_BROWSING_BLACKLIST, result.source);

  // Check that the origin is not under embargo for a different permission.
  result =
      autoblocker()->GetEmbargoResult(url, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Confirm embargo status during the embargo period.
  clock()->Advance(base::TimeDelta::FromDays(5));
  result =
      autoblocker()->GetEmbargoResult(url, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::SAFE_BROWSING_BLACKLIST, result.source);

  // Check embargo is lifted on expiry day. A small offset after the exact
  // embargo expiration date has been added to account for any precision errors
  // when removing the date stored as a double from the permission dictionary.
  clock()->Advance(base::TimeDelta::FromHours(3 * 24 + 1));
  result =
      autoblocker()->GetEmbargoResult(url, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Check embargo is lifted well after the expiry day.
  clock()->Advance(base::TimeDelta::FromDays(1));
  result =
      autoblocker()->GetEmbargoResult(url, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Place under embargo again and verify the embargo status.
  PlaceUnderBlacklistEmbargo(url, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  clock()->Advance(base::TimeDelta::FromDays(1));
  result =
      autoblocker()->GetEmbargoResult(url, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::SAFE_BROWSING_BLACKLIST, result.source);
}

// Tests the alternating pattern of the block on multiple dismiss behaviour. On
// N dismissals, the origin to be embargoed for the requested permission and
// automatically blocked. Each time the embargo is lifted, the site gets another
// chance to request the permission, but if it is again dismissed it is placed
// under embargo again and its permission requests blocked.
TEST_F(PermissionDecisionAutoBlockerUnitTest, TestDismissEmbargoBackoff) {
  GURL url("https://www.google.com");
  clock()->SetNow(base::Time::Now());
  base::HistogramTester histograms;

  // Record some dismisses.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, CONTENT_SETTINGS_TYPE_GEOLOCATION));

  // A request with < 3 prior dismisses should not be automatically blocked.
  PermissionResult result =
      autoblocker()->GetEmbargoResult(url, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // After the 3rd dismiss subsequent permission requests should be autoblocked.
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  result =
      autoblocker()->GetEmbargoResult(url, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);

  histograms.ExpectTotalCount("Permissions.AutoBlocker.SafeBrowsingResponse",
                              0);
  histograms.ExpectTotalCount(
      "Permissions.AutoBlocker.SafeBrowsingResponseTime", 0);
  // Accelerate time forward, check that the embargo status is lifted and the
  // request won't be automatically blocked.
  clock()->Advance(base::TimeDelta::FromDays(8));
  result =
      autoblocker()->GetEmbargoResult(url, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Record another dismiss, subsequent requests should be autoblocked again.
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  result =
      autoblocker()->GetEmbargoResult(url, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);

  // Accelerate time again, check embargo is lifted and another permission
  // request is let through.
  clock()->Advance(base::TimeDelta::FromDays(8));
  result =
      autoblocker()->GetEmbargoResult(url, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  // Record another dismiss, subsequent requests should be autoblocked again.
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  result =
      autoblocker()->GetEmbargoResult(url, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);
  histograms.ExpectTotalCount("Permissions.AutoBlocker.SafeBrowsingResponse",
                              0);
  histograms.ExpectTotalCount(
      "Permissions.AutoBlocker.SafeBrowsingResponseTime", 0);
}

// Test the logic for a combination of blacklisting and dismissal embargo.
TEST_F(PermissionDecisionAutoBlockerUnitTest, TestExpiredBlacklistEmbargo) {
  GURL url("https://www.google.com");
  clock()->SetNow(base::Time::Now());

  // Place under blacklist embargo and check the status.
  PlaceUnderBlacklistEmbargo(url, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  clock()->Advance(base::TimeDelta::FromDays(5));
  PermissionResult result =
      autoblocker()->GetEmbargoResult(url, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::SAFE_BROWSING_BLACKLIST, result.source);

  // Record dismisses to place it under dismissal embargo.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url, CONTENT_SETTINGS_TYPE_GEOLOCATION));

  // Accelerate time to a point where the blacklist embargo should be expired
  // and check that dismissal embargo is still set.
  clock()->Advance(base::TimeDelta::FromDays(3));
  result =
      autoblocker()->GetEmbargoResult(url, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::MULTIPLE_DISMISSALS, result.source);
}

TEST_F(PermissionDecisionAutoBlockerUnitTest, TestSafeBrowsingTimeout) {
  GURL url("https://www.google.com");
  clock()->SetNow(base::Time::Now());
  base::HistogramTester histograms;

  scoped_refptr<MockSafeBrowsingDatabaseManager> db_manager =
      new MockSafeBrowsingDatabaseManager(false /* perform_callback */,
                                          true /* enabled */);
  std::set<std::string> blacklisted_permissions{"GEOLOCATION"};
  db_manager->BlacklistUrlPermissions(url, blacklisted_permissions);
  SetSafeBrowsingDatabaseManagerAndTimeoutForTesting(db_manager,
                                                     0 /* timeout in ms */);

  CheckSafeBrowsingBlacklist(url, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  EXPECT_TRUE(callback_was_run());
  EXPECT_FALSE(last_embargoed_status());

  PermissionResult result =
      autoblocker()->GetEmbargoResult(url, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_ASK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::UNSPECIFIED, result.source);

  histograms.ExpectUniqueSample("Permissions.AutoBlocker.SafeBrowsingResponse",
                                static_cast<int>(SafeBrowsingResponse::TIMEOUT),
                                1);
  histograms.ExpectTotalCount(
      "Permissions.AutoBlocker.SafeBrowsingResponseTime", 1);
  db_manager->SetPerformCallback(true);
  SetSafeBrowsingDatabaseManagerAndTimeoutForTesting(db_manager,
                                                     2000 /* timeout in ms */);

  clock()->Advance(base::TimeDelta::FromDays(1));
  CheckSafeBrowsingBlacklist(url, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  EXPECT_TRUE(callback_was_run());
  EXPECT_TRUE(last_embargoed_status());
  histograms.ExpectTotalCount("Permissions.AutoBlocker.SafeBrowsingResponse",
                              2);
  histograms.ExpectTotalCount(
      "Permissions.AutoBlocker.SafeBrowsingResponseTime", 2);
  histograms.ExpectBucketCount(
      "Permissions.AutoBlocker.SafeBrowsingResponse",
      static_cast<int>(SafeBrowsingResponse::BLACKLISTED), 1);
  clock()->Advance(base::TimeDelta::FromDays(1));
  result =
      autoblocker()->GetEmbargoResult(url, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, result.content_setting);
  EXPECT_EQ(PermissionStatusSource::SAFE_BROWSING_BLACKLIST, result.source);
}

// TODO(raymes): See crbug.com/681709. Remove after M60.
TEST_F(PermissionDecisionAutoBlockerUnitTest,
       MigrateNoDecisionCountToPermissionAutoBlockerData) {
  GURL url("https://www.google.com");
  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());

  // Write to the old content setting.
  base::DictionaryValue permissions_dict;
  permissions_dict.SetInteger(GetDismissKey(), 100);
  permissions_dict.SetInteger(GetIgnoreKey(), 50);

  base::DictionaryValue origin_dict;
  origin_dict.Set(
      PermissionUtil::GetPermissionString(CONTENT_SETTINGS_TYPE_GEOLOCATION),
      permissions_dict.CreateDeepCopy());
  map->SetWebsiteSettingDefaultScope(
      url, GURL(), CONTENT_SETTINGS_TYPE_PROMPT_NO_DECISION_COUNT,
      std::string(), origin_dict.CreateDeepCopy());

  // Nothing should be migrated yet, so the current values should be 0.
  EXPECT_EQ(0, autoblocker()->GetDismissCount(
                   url, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_EQ(
      0, autoblocker()->GetIgnoreCount(url, CONTENT_SETTINGS_TYPE_GEOLOCATION));

  // Trigger pref migration which happens at the creation of the
  // HostContentSettingsMap.
  {
    scoped_refptr<HostContentSettingsMap> temp_map(new HostContentSettingsMap(
        profile()->GetPrefs(), false /* is_incognito_profile */,
        false /* is_guest_profile */));
    temp_map->ShutdownOnUIThread();
  }

  // The values should now be migrated.
  EXPECT_EQ(100, autoblocker()->GetDismissCount(
                     url, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_EQ(50, autoblocker()->GetIgnoreCount(
                    url, CONTENT_SETTINGS_TYPE_GEOLOCATION));

  // The old pref should be deleted.
  std::unique_ptr<base::DictionaryValue> old_dict =
      base::DictionaryValue::From(map->GetWebsiteSetting(
          url, GURL(), CONTENT_SETTINGS_TYPE_PROMPT_NO_DECISION_COUNT,
          std::string(), nullptr));
  EXPECT_EQ(nullptr, old_dict);

  // Write to the old content setting again, but with different numbers.
  permissions_dict.SetInteger(GetDismissKey(), 99);
  permissions_dict.SetInteger(GetIgnoreKey(), 99);

  origin_dict.Set(
      PermissionUtil::GetPermissionString(CONTENT_SETTINGS_TYPE_GEOLOCATION),
      permissions_dict.CreateDeepCopy());
  map->SetWebsiteSettingDefaultScope(
      url, GURL(), CONTENT_SETTINGS_TYPE_PROMPT_NO_DECISION_COUNT,
      std::string(), origin_dict.CreateDeepCopy());

  // Ensure that migrating again does nothing.
  {
    scoped_refptr<HostContentSettingsMap> temp_map(new HostContentSettingsMap(
        profile()->GetPrefs(), false /* is_incognito_profile */,
        false /* is_guest_profile */));
    temp_map->ShutdownOnUIThread();
  }

  EXPECT_EQ(100, autoblocker()->GetDismissCount(
                     url, CONTENT_SETTINGS_TYPE_GEOLOCATION));
  EXPECT_EQ(50, autoblocker()->GetIgnoreCount(
                    url, CONTENT_SETTINGS_TYPE_GEOLOCATION));
}

// Test that a blacklisted permission should not be autoblocked if the database
// manager is disabled.
TEST_F(PermissionDecisionAutoBlockerUnitTest, TestDisabledDatabaseManager) {
  GURL url("https://www.google.com");
  scoped_refptr<MockSafeBrowsingDatabaseManager> db_manager =
      new MockSafeBrowsingDatabaseManager(true /* perform_callback */,
                                          false /* enabled */);
  std::set<std::string> blacklisted_permissions{"GEOLOCATION"};
  db_manager->BlacklistUrlPermissions(url, blacklisted_permissions);
  SetSafeBrowsingDatabaseManagerAndTimeoutForTesting(db_manager,
                                                     2000 /* timeout in ms */);
  CheckSafeBrowsingBlacklist(url, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  EXPECT_TRUE(callback_was_run());
  EXPECT_FALSE(last_embargoed_status());
}

TEST_F(PermissionDecisionAutoBlockerUnitTest, TestSafeBrowsingResponse) {
  GURL url("https://www.google.com");
  clock()->SetNow(base::Time::Now());
  base::HistogramTester histograms;

  scoped_refptr<MockSafeBrowsingDatabaseManager> db_manager =
      new MockSafeBrowsingDatabaseManager(true /* perform_callback */,
                                          true /* enabled */);
  std::set<std::string> blacklisted_permissions{"GEOLOCATION"};
  db_manager->BlacklistUrlPermissions(url, blacklisted_permissions);
  SetSafeBrowsingDatabaseManagerAndTimeoutForTesting(db_manager,
                                                     0 /* timeout in ms */);

  CheckSafeBrowsingBlacklist(url, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  EXPECT_FALSE(last_embargoed_status());
  histograms.ExpectUniqueSample(
      "Permissions.AutoBlocker.SafeBrowsingResponse",
      static_cast<int>(SafeBrowsingResponse::NOT_BLACKLISTED), 1);
}
