// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/important_sites_util.h"

#include <memory>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_helper.h"
#include "chrome/browser/engagement/site_engagement_metrics.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/history/core/browser/history_database_params.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/test_history_database.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const size_t kNumImportantSites = 5;
base::FilePath g_temp_history_dir;

scoped_ptr<KeyedService> BuildTestHistoryService(
    content::BrowserContext* context) {
  scoped_ptr<history::HistoryService> service(new history::HistoryService());
  service->Init(history::TestHistoryDatabaseParamsForPath(g_temp_history_dir));
  return std::move(service);
}

}  // namespace

class ImportantSitesUtilTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    g_temp_history_dir = temp_dir_.path();
    HistoryServiceFactory::GetInstance()->SetTestingFactory(
        profile(), &BuildTestHistoryService);
    SiteEngagementScore::SetParamValuesForTesting();
  }

  void AddContentSetting(ContentSettingsType type,
                         ContentSetting setting,
                         const GURL& origin) {
    ContentSettingsForOneType settings_list;

    HostContentSettingsMapFactory::GetForProfile(profile())
        ->SetContentSettingCustomScope(
            ContentSettingsPattern::FromURLNoWildcard(origin),
            ContentSettingsPattern::Wildcard(),
            CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
            content_settings::ResourceIdentifier(), setting);
  }

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(ImportantSitesUtilTest, TestNoImportantSites) {
  EXPECT_TRUE(ImportantSitesUtil::GetImportantRegisterableDomains(
                  profile(), kNumImportantSites)
                  .empty());
}

TEST_F(ImportantSitesUtilTest, NotificationsThenEngagement) {
  SiteEngagementService* service =
      SiteEngagementServiceFactory::GetForProfile(profile());
  ASSERT_TRUE(service);

  GURL url1("http://www.google.com/");
  GURL url2("https://www.google.com/");
  GURL url3("https://drive.google.com/");
  GURL url4("https://www.chrome.com/");
  GURL url5("https://www.example.com/");
  GURL url6("https://youtube.com/");
  GURL url7("https://foo.bar/");

  service->ResetScoreForURL(url1, 5);
  service->ResetScoreForURL(url2, 2);  // Below medium engagement (5).
  service->ResetScoreForURL(url3, 7);
  service->ResetScoreForURL(url4, 8);
  service->ResetScoreForURL(url5, 9);
  service->ResetScoreForURL(url6, 1);  // Below the medium engagement (5).
  service->ResetScoreForURL(url7, 11);

  // Here we should have:
  // 1: removed domains below minimum engagement,
  // 2: combined the google.com entries, and
  // 3: sorted by the score.
  std::vector<std::string> expected_sorted_domains = {
      "foo.bar", "example.com", "chrome.com", "google.com"};
  EXPECT_THAT(ImportantSitesUtil::GetImportantRegisterableDomains(
                  profile(), kNumImportantSites),
              ::testing::ElementsAreArray(expected_sorted_domains));

  // Test that notifications get moved to the front.
  AddContentSetting(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_ALLOW,
                    url6);
  // BLOCK'ed sites don't count. We want to make sure we only bump sites that
  // were granted the permsion.
  AddContentSetting(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, CONTENT_SETTING_BLOCK,
                    url1);

  // Same as above, but the site with notifications should be at the front.
  expected_sorted_domains = {"youtube.com", "foo.bar", "example.com",
                             "chrome.com", "google.com"};
  EXPECT_THAT(ImportantSitesUtil::GetImportantRegisterableDomains(
                  profile(), kNumImportantSites),
              ::testing::ElementsAreArray(expected_sorted_domains));
}
