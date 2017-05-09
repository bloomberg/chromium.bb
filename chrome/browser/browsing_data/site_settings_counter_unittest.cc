// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/site_settings_counter.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SiteSettingsCounterTest : public testing::Test {
 public:
  SiteSettingsCounterTest() {
    base::test::ScopedFeatureList feature_list;
    // Enable tabsInCbd to activate timestamp recording for content-settings.
    feature_list.InitAndEnableFeature(features::kTabsInCbd);
    profile_ = base::MakeUnique<TestingProfile>();
    map_ = HostContentSettingsMapFactory::GetForProfile(profile());
  }

  Profile* profile() { return profile_.get(); }

  HostContentSettingsMap* map() { return map_.get(); }

  void SetSiteSettingsDeletionPref(bool value) {
    profile()->GetPrefs()->SetBoolean(browsing_data::prefs::kDeleteSiteSettings,
                                      value);
  }

  void SetDeletionPeriodPref(browsing_data::TimePeriod period) {
    profile()->GetPrefs()->SetInteger(browsing_data::prefs::kDeleteTimePeriod,
                                      static_cast<int>(period));
  }

  browsing_data::BrowsingDataCounter::ResultInt GetResult() {
    DCHECK(finished_);
    return result_;
  }

  void Callback(
      std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
    DCHECK(result->Finished());
    finished_ = result->Finished();

    result_ = static_cast<browsing_data::BrowsingDataCounter::FinishedResult*>(
                  result.get())
                  ->Value();
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;

  scoped_refptr<HostContentSettingsMap> map_;
  bool finished_;
  browsing_data::BrowsingDataCounter::ResultInt result_;
};

// Tests that the counter correctly counts each setting.
TEST_F(SiteSettingsCounterTest, Count) {
  map()->SetContentSettingDefaultScope(
      GURL("http://www.google.com"), GURL("http://www.google.com"),
      CONTENT_SETTINGS_TYPE_POPUPS, std::string(), CONTENT_SETTING_ALLOW);
  map()->SetContentSettingDefaultScope(
      GURL("http://maps.google.com"), GURL("http://maps.google.com"),
      CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(), CONTENT_SETTING_ALLOW);

  browsing_data::SiteSettingsCounter counter(map());
  counter.Init(
      profile()->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::Bind(&SiteSettingsCounterTest::Callback, base::Unretained(this)));
  counter.Restart();

  EXPECT_EQ(2, GetResult());
}

// Test that the counter counts correctly when using a time period.
TEST_F(SiteSettingsCounterTest, CountWithTimePeriod) {
  auto test_clock = base::MakeUnique<base::SimpleTestClock>();
  base::SimpleTestClock* clock = test_clock.get();
  map()->SetClockForTesting(std::move(test_clock));

  // Create a setting at Now()-90min.
  clock->SetNow(base::Time::Now() - base::TimeDelta::FromMinutes(90));
  map()->SetContentSettingDefaultScope(
      GURL("http://www.google.com"), GURL("http://www.google.com"),
      CONTENT_SETTINGS_TYPE_POPUPS, std::string(), CONTENT_SETTING_ALLOW);

  // Create a setting at Now()-30min.
  clock->SetNow(base::Time::Now() - base::TimeDelta::FromMinutes(30));
  map()->SetContentSettingDefaultScope(
      GURL("http://maps.google.com"), GURL("http://maps.google.com"),
      CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(), CONTENT_SETTING_ALLOW);

  clock->SetNow(base::Time::Now());
  browsing_data::SiteSettingsCounter counter(map());
  counter.Init(
      profile()->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::Bind(&SiteSettingsCounterTest::Callback, base::Unretained(this)));
  // Only one of the settings was created in the last hour.
  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_HOUR);
  EXPECT_EQ(1, GetResult());
  // Both settings were created during the last day.
  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_DAY);
  EXPECT_EQ(2, GetResult());
}

// Tests that the counter doesn't count website settings
TEST_F(SiteSettingsCounterTest, OnlyCountContentSettings) {
  map()->SetContentSettingDefaultScope(
      GURL("http://www.google.com"), GURL("http://www.google.com"),
      CONTENT_SETTINGS_TYPE_POPUPS, std::string(), CONTENT_SETTING_ALLOW);
  map()->SetWebsiteSettingDefaultScope(
      GURL("http://maps.google.com"), GURL(),
      CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, std::string(),
      base::MakeUnique<base::DictionaryValue>());

  browsing_data::SiteSettingsCounter counter(map());
  counter.Init(
      profile()->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::Bind(&SiteSettingsCounterTest::Callback, base::Unretained(this)));
  counter.Restart();

  EXPECT_EQ(1, GetResult());
}

// Tests that the counter counts settings with the same pattern only
// once.
TEST_F(SiteSettingsCounterTest, OnlyCountPatternOnce) {
  map()->SetContentSettingDefaultScope(
      GURL("http://www.google.com"), GURL("http://www.google.com"),
      CONTENT_SETTINGS_TYPE_POPUPS, std::string(), CONTENT_SETTING_ALLOW);
  map()->SetContentSettingDefaultScope(
      GURL("http://www.google.com"), GURL("http://www.google.com"),
      CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(), CONTENT_SETTING_ALLOW);

  browsing_data::SiteSettingsCounter counter(map());
  counter.Init(
      profile()->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::Bind(&SiteSettingsCounterTest::Callback, base::Unretained(this)));
  counter.Restart();

  EXPECT_EQ(1, GetResult());
}

// Tests that the counter starts counting automatically when the deletion
// pref changes to true.
TEST_F(SiteSettingsCounterTest, PrefChanged) {
  SetSiteSettingsDeletionPref(false);
  map()->SetContentSettingDefaultScope(
      GURL("http://www.google.com"), GURL("http://www.google.com"),
      CONTENT_SETTINGS_TYPE_POPUPS, std::string(), CONTENT_SETTING_ALLOW);

  browsing_data::SiteSettingsCounter counter(map());
  counter.Init(
      profile()->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::Bind(&SiteSettingsCounterTest::Callback, base::Unretained(this)));
  SetSiteSettingsDeletionPref(true);
  EXPECT_EQ(1, GetResult());
}

// Tests that changing the deletion period restarts the counting.
TEST_F(SiteSettingsCounterTest, PeriodChanged) {
  map()->SetContentSettingDefaultScope(
      GURL("http://www.google.com"), GURL("http://www.google.com"),
      CONTENT_SETTINGS_TYPE_POPUPS, std::string(), CONTENT_SETTING_ALLOW);

  browsing_data::SiteSettingsCounter counter(map());
  counter.Init(
      profile()->GetPrefs(), browsing_data::ClearBrowsingDataTab::ADVANCED,
      base::Bind(&SiteSettingsCounterTest::Callback, base::Unretained(this)));

  SetDeletionPeriodPref(browsing_data::TimePeriod::LAST_HOUR);
  EXPECT_EQ(1, GetResult());
}

}  // namespace
