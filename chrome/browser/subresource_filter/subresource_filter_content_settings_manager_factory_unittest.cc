// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/subresource_filter_content_settings_manager_factory.h"

#include "base/macros.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kActionsHistogram[] = "SubresourceFilter.Actions";

class SubresourceFilterContentSettingsManagerFactoryTest
    : public testing::Test {
 public:
  SubresourceFilterContentSettingsManagerFactoryTest() {}

  void SetUp() override {
    SubresourceFilterContentSettingsManagerFactory::EnsureForProfile(
        &testing_profile_);
    histogram_tester().ExpectTotalCount(kActionsHistogram, 0);
  }

  HostContentSettingsMap* GetSettingsMap() {
    return HostContentSettingsMapFactory::GetForProfile(&testing_profile_);
  }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile testing_profile_;
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterContentSettingsManagerFactoryTest);
};

TEST_F(SubresourceFilterContentSettingsManagerFactoryTest, IrrelevantSetting) {
  GetSettingsMap()->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_POPUPS,
                                             CONTENT_SETTING_ALLOW);
  histogram_tester().ExpectTotalCount(kActionsHistogram, 0);
}

TEST_F(SubresourceFilterContentSettingsManagerFactoryTest, DefaultSetting) {
  GetSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_SUBRESOURCE_FILTER, CONTENT_SETTING_ALLOW);
  histogram_tester().ExpectBucketCount(kActionsHistogram,
                                       kActionContentSettingsAllowedGlobal, 1);

  GetSettingsMap()->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_SUBRESOURCE_FILTER, CONTENT_SETTING_BLOCK);
  histogram_tester().ExpectTotalCount(kActionsHistogram, 2);
  histogram_tester().ExpectBucketCount(kActionsHistogram,
                                       kActionContentSettingsBlockedGlobal, 1);
}

TEST_F(SubresourceFilterContentSettingsManagerFactoryTest, UrlSetting) {
  GURL url("https://www.example.test/");

  GetSettingsMap()->SetContentSettingDefaultScope(
      url, url, CONTENT_SETTINGS_TYPE_SUBRESOURCE_FILTER, std::string(),
      CONTENT_SETTING_BLOCK);
  histogram_tester().ExpectBucketCount(kActionsHistogram,
                                       kActionContentSettingsBlocked, 1);

  GetSettingsMap()->SetContentSettingDefaultScope(
      url, url, CONTENT_SETTINGS_TYPE_SUBRESOURCE_FILTER, std::string(),
      CONTENT_SETTING_ALLOW);
  histogram_tester().ExpectTotalCount(kActionsHistogram, 2);
  histogram_tester().ExpectBucketCount(kActionsHistogram,
                                       kActionContentSettingsAllowed, 1);
}

TEST_F(SubresourceFilterContentSettingsManagerFactoryTest, WildcardUpdate) {
  ContentSettingsPattern primary_pattern =
      ContentSettingsPattern::FromString("[*.]example.test");
  ContentSettingsPattern secondary_pattern = ContentSettingsPattern::Wildcard();

  GetSettingsMap()->SetContentSettingCustomScope(
      primary_pattern, secondary_pattern,
      CONTENT_SETTINGS_TYPE_SUBRESOURCE_FILTER, std::string(),
      CONTENT_SETTING_BLOCK);
  histogram_tester().ExpectBucketCount(kActionsHistogram,
                                       kActionContentSettingsWildcardUpdate, 1);

  GetSettingsMap()->SetContentSettingCustomScope(
      primary_pattern, secondary_pattern,
      CONTENT_SETTINGS_TYPE_SUBRESOURCE_FILTER, std::string(),
      CONTENT_SETTING_ALLOW);
  histogram_tester().ExpectTotalCount(kActionsHistogram, 2);
  histogram_tester().ExpectBucketCount(kActionsHistogram,
                                       kActionContentSettingsWildcardUpdate, 2);
}

}  // namespace
