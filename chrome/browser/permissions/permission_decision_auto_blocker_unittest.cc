// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_decision_auto_blocker.h"

#include <map>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_util.h"
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

}  // namespace

class MockSafeBrowsingDatabaseManager
    : public safe_browsing::TestSafeBrowsingDatabaseManager {
 public:
  explicit MockSafeBrowsingDatabaseManager(bool perform_callback)
      : perform_callback_(perform_callback) {}

  bool CheckApiBlacklistUrl(
      const GURL& url,
      safe_browsing::SafeBrowsingDatabaseManager::Client* client) override {
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
  std::map<GURL, std::set<std::string>> permissions_blacklist_;

  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingDatabaseManager);
};

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
  }

  void SetSafeBrowsingDatabaseManagerAndTimeoutForTesting(
      scoped_refptr<MockSafeBrowsingDatabaseManager> db_manager,
      int timeout) {
    autoblocker_->SetSafeBrowsingDatabaseManagerAndTimeoutForTesting(db_manager,
                                                                     timeout);
  }

  void UpdateEmbargoedStatus(content::PermissionType permission,
                             const GURL& url) {
    base::RunLoop run_loop;
    autoblocker_->UpdateEmbargoedStatus(
        permission, url, nullptr,
        base::Bind(&PermissionDecisionAutoBlockerUnitTest::SetLastEmbargoStatus,
                   base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Manually placing an (origin, permission) pair under embargo for
  // blacklisting. To embargo on dismissals, RecordDismissAndEmbargo can be
  // used.
  void PlaceUnderBlacklistEmbargo(content::PermissionType permission,
                                  const GURL& url) {
    autoblocker_->PlaceUnderEmbargo(
        permission, url,
        PermissionDecisionAutoBlocker::kPermissionBlacklistEmbargoKey);
  }

  PermissionDecisionAutoBlocker* autoblocker() { return autoblocker_; }

  void SetLastEmbargoStatus(base::Closure quit_closure, bool status) {
    last_embargoed_status_ = status;
    if (quit_closure) {
      quit_closure.Run();
      quit_closure.Reset();
    }
  }

  bool last_embargoed_status() { return last_embargoed_status_; }

  base::SimpleTestClock* clock() { return clock_; }

 private:
  PermissionDecisionAutoBlocker* autoblocker_;
  base::test::ScopedFeatureList feature_list_;
  base::SimpleTestClock* clock_;
  bool last_embargoed_status_;
};

TEST_F(PermissionDecisionAutoBlockerUnitTest, RemoveCountsByUrl) {
  GURL url1("https://www.google.com");
  GURL url2("https://www.example.com");

  // Record some dismissals.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(1, autoblocker()->GetDismissCount(
                   url1, content::PermissionType::GEOLOCATION));

  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(2, autoblocker()->GetDismissCount(
                   url1, content::PermissionType::GEOLOCATION));

  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url1, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(3, autoblocker()->GetDismissCount(
                   url1, content::PermissionType::GEOLOCATION));

  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url2, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(1, autoblocker()->GetDismissCount(
                   url2, content::PermissionType::GEOLOCATION));

  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, content::PermissionType::NOTIFICATIONS));
  EXPECT_EQ(1, autoblocker()->GetDismissCount(
                   url1, content::PermissionType::NOTIFICATIONS));

  // Record some ignores.
  EXPECT_EQ(1, autoblocker()->RecordIgnore(
                   url1, content::PermissionType::MIDI_SYSEX));
  EXPECT_EQ(1, autoblocker()->RecordIgnore(
                   url1, content::PermissionType::DURABLE_STORAGE));
  EXPECT_EQ(1, autoblocker()->RecordIgnore(
                   url2, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(2, autoblocker()->RecordIgnore(
                   url2, content::PermissionType::GEOLOCATION));

  autoblocker()->RemoveCountsByUrl(base::Bind(&FilterGoogle));

  // Expect that url1's actions are gone, but url2's remain.
  EXPECT_EQ(0, autoblocker()->GetDismissCount(
                   url1, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(0, autoblocker()->GetDismissCount(
                   url1, content::PermissionType::NOTIFICATIONS));
  EXPECT_EQ(0, autoblocker()->GetIgnoreCount(
                   url1, content::PermissionType::MIDI_SYSEX));
  EXPECT_EQ(0, autoblocker()->GetIgnoreCount(
                   url1, content::PermissionType::DURABLE_STORAGE));

  EXPECT_EQ(1, autoblocker()->GetDismissCount(
                   url2, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(2, autoblocker()->GetIgnoreCount(
                   url2, content::PermissionType::GEOLOCATION));

  // Add some more actions.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(1, autoblocker()->GetDismissCount(
                   url1, content::PermissionType::GEOLOCATION));

  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url1, content::PermissionType::NOTIFICATIONS));
  EXPECT_EQ(1, autoblocker()->GetDismissCount(
                   url1, content::PermissionType::NOTIFICATIONS));

  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url2, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(2, autoblocker()->GetDismissCount(
                   url2, content::PermissionType::GEOLOCATION));

  EXPECT_EQ(1, autoblocker()->RecordIgnore(
                   url1, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(1, autoblocker()->RecordIgnore(
                   url1, content::PermissionType::NOTIFICATIONS));
  EXPECT_EQ(1, autoblocker()->RecordIgnore(
                   url1, content::PermissionType::DURABLE_STORAGE));
  EXPECT_EQ(1, autoblocker()->RecordIgnore(
                   url2, content::PermissionType::MIDI_SYSEX));

  // Remove everything and expect that it's all gone.
  autoblocker()->RemoveCountsByUrl(base::Bind(&FilterAll));

  EXPECT_EQ(0, autoblocker()->GetDismissCount(
                   url1, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(0, autoblocker()->GetDismissCount(
                   url1, content::PermissionType::NOTIFICATIONS));
  EXPECT_EQ(0, autoblocker()->GetDismissCount(
                   url2, content::PermissionType::GEOLOCATION));

  EXPECT_EQ(0, autoblocker()->GetIgnoreCount(
                   url1, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(0, autoblocker()->GetIgnoreCount(
                   url1, content::PermissionType::NOTIFICATIONS));
  EXPECT_EQ(0, autoblocker()->GetIgnoreCount(
                   url2, content::PermissionType::GEOLOCATION));
  EXPECT_EQ(0, autoblocker()->GetIgnoreCount(
                   url2, content::PermissionType::DURABLE_STORAGE));
  EXPECT_EQ(0, autoblocker()->GetIgnoreCount(
                   url2, content::PermissionType::MIDI_SYSEX));
}

// Test that an origin that has been blacklisted for a permission is embargoed.
TEST_F(PermissionDecisionAutoBlockerUnitTest, TestUpdateEmbargoBlacklist) {
  GURL url("https://www.google.com");

  scoped_refptr<MockSafeBrowsingDatabaseManager> db_manager =
      new MockSafeBrowsingDatabaseManager(true /* perform_callback */);
  std::set<std::string> blacklisted_permissions{"GEOLOCATION"};
  db_manager->BlacklistUrlPermissions(url, blacklisted_permissions);
  SetSafeBrowsingDatabaseManagerAndTimeoutForTesting(db_manager,
                                                     2000 /* timeout in ms */);

  UpdateEmbargoedStatus(content::PermissionType::GEOLOCATION, url);
  EXPECT_TRUE(last_embargoed_status());
}

// Check that IsUnderEmbargo returns the correct value when the embargo is set
// and expires.
TEST_F(PermissionDecisionAutoBlockerUnitTest, CheckEmbargoStatus) {
  GURL url("https://www.google.com");
  clock()->SetNow(base::Time::Now());

  PlaceUnderBlacklistEmbargo(content::PermissionType::GEOLOCATION, url);
  EXPECT_TRUE(
      autoblocker()->IsUnderEmbargo(content::PermissionType::GEOLOCATION, url));

  // Check that the origin is not under embargo for a different permission.
  EXPECT_FALSE(autoblocker()->IsUnderEmbargo(
      content::PermissionType::NOTIFICATIONS, url));

  // Confirm embargo status during the embargo period.
  clock()->Advance(base::TimeDelta::FromDays(5));
  EXPECT_TRUE(
      autoblocker()->IsUnderEmbargo(content::PermissionType::GEOLOCATION, url));

  // Check embargo is lifted on expiry day. A small offset after the exact
  // embargo expiration date has been added to account for any precision errors
  // when removing the date stored as a double from the permission dictionary.
  clock()->Advance(base::TimeDelta::FromHours(3 * 24 + 1));
  EXPECT_FALSE(
      autoblocker()->IsUnderEmbargo(content::PermissionType::GEOLOCATION, url));

  // Check embargo is lifted well after the expiry day.
  clock()->Advance(base::TimeDelta::FromDays(1));
  EXPECT_FALSE(
      autoblocker()->IsUnderEmbargo(content::PermissionType::GEOLOCATION, url));

  // Place under embargo again and verify the embargo status.
  PlaceUnderBlacklistEmbargo(content::PermissionType::NOTIFICATIONS, url);
  clock()->Advance(base::TimeDelta::FromDays(1));
  EXPECT_TRUE(autoblocker()->IsUnderEmbargo(
      content::PermissionType::NOTIFICATIONS, url));
}

// Tests the alternating pattern of the block on multiple dismiss behaviour. On
// N dismissals, the origin to be embargoed for the requested permission and
// automatically blocked. Each time the embargo is lifted, the site gets another
// chance to request the permission, but if it is again dismissed it is placed
// under embargo again and its permission requests blocked.
TEST_F(PermissionDecisionAutoBlockerUnitTest, TestDismissEmbargoBackoff) {
  GURL url("https://www.google.com");
  clock()->SetNow(base::Time::Now());

  // Record some dismisses.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, content::PermissionType::GEOLOCATION));
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, content::PermissionType::GEOLOCATION));

  // A request with < 3 prior dismisses should not be automatically blocked.
  EXPECT_FALSE(
      autoblocker()->IsUnderEmbargo(content::PermissionType::GEOLOCATION, url));

  // After the 3rd dismiss subsequent permission requests should be autoblocked.
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url, content::PermissionType::GEOLOCATION));
  EXPECT_TRUE(
      autoblocker()->IsUnderEmbargo(content::PermissionType::GEOLOCATION, url));

  // Accelerate time forward, check that the embargo status is lifted and the
  // request won't be automatically blocked.
  clock()->Advance(base::TimeDelta::FromDays(8));
  EXPECT_FALSE(
      autoblocker()->IsUnderEmbargo(content::PermissionType::GEOLOCATION, url));

  // Record another dismiss, subsequent requests should be autoblocked again.
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url, content::PermissionType::GEOLOCATION));
  EXPECT_TRUE(
      autoblocker()->IsUnderEmbargo(content::PermissionType::GEOLOCATION, url));

  // Accelerate time again, check embargo is lifted and another permission
  // request is let through.
  clock()->Advance(base::TimeDelta::FromDays(8));
  EXPECT_FALSE(
      autoblocker()->IsUnderEmbargo(content::PermissionType::GEOLOCATION, url));
}

// Test the logic for a combination of blacklisting and dismissal embargo.
TEST_F(PermissionDecisionAutoBlockerUnitTest, TestExpiredBlacklistEmbargo) {
  GURL url("https://www.google.com");
  clock()->SetNow(base::Time::Now());

  // Place under blacklist embargo and check the status.
  PlaceUnderBlacklistEmbargo(content::PermissionType::GEOLOCATION, url);
  clock()->Advance(base::TimeDelta::FromDays(5));
  EXPECT_TRUE(
      autoblocker()->IsUnderEmbargo(content::PermissionType::GEOLOCATION, url));

  // Record dismisses to place it under dismissal embargo.
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, content::PermissionType::GEOLOCATION));
  EXPECT_FALSE(autoblocker()->RecordDismissAndEmbargo(
      url, content::PermissionType::GEOLOCATION));
  EXPECT_TRUE(autoblocker()->RecordDismissAndEmbargo(
      url, content::PermissionType::GEOLOCATION));

  // Accelerate time to a point where the blacklist embargo should be expired
  // and check that dismissal embargo is still set.
  clock()->Advance(base::TimeDelta::FromDays(3));
  EXPECT_TRUE(
      autoblocker()->IsUnderEmbargo(content::PermissionType::GEOLOCATION, url));
}

TEST_F(PermissionDecisionAutoBlockerUnitTest, TestSafeBrowsingTimeout) {
  GURL url("https://www.google.com");
  clock()->SetNow(base::Time::Now());

  scoped_refptr<MockSafeBrowsingDatabaseManager> db_manager =
      new MockSafeBrowsingDatabaseManager(false /* perform_callback */);
  std::set<std::string> blacklisted_permissions{"GEOLOCATION"};
  db_manager->BlacklistUrlPermissions(url, blacklisted_permissions);
  SetSafeBrowsingDatabaseManagerAndTimeoutForTesting(db_manager,
                                                     0 /* timeout in ms */);

  UpdateEmbargoedStatus(content::PermissionType::GEOLOCATION, url);
  EXPECT_FALSE(last_embargoed_status());
  EXPECT_FALSE(
      autoblocker()->IsUnderEmbargo(content::PermissionType::GEOLOCATION, url));
  db_manager->SetPerformCallback(true);
  SetSafeBrowsingDatabaseManagerAndTimeoutForTesting(db_manager,
                                                     2000 /* timeout in ms */);

  clock()->Advance(base::TimeDelta::FromDays(1));
  UpdateEmbargoedStatus(content::PermissionType::GEOLOCATION, url);
  EXPECT_TRUE(last_embargoed_status());

  clock()->Advance(base::TimeDelta::FromDays(1));
  EXPECT_TRUE(
      autoblocker()->IsUnderEmbargo(content::PermissionType::GEOLOCATION, url));
}
