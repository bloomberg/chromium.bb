// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/chrome_plugin_service_filter.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_score.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/plugins/flash_temporary_permission_tracker.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/plugins/plugins_field_trial.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "content/public/test/test_utils.h"
#include "url/origin.h"

namespace {

const char kTrialName[] = "PreferHtmlOverPlugins";
const char kGroupName[] = "Group1";

}  // namespace

class ChromePluginServiceFilterTest : public ChromeRenderViewHostTestHarness {
 public:
  ChromePluginServiceFilterTest()
      : ChromeRenderViewHostTestHarness(),
        filter_(nullptr),
        flash_plugin_path_(FILE_PATH_LITERAL("/path/to/flash")) {}

  bool IsPluginAvailable(const GURL& plugin_content_url,
                         const url::Origin& main_frame_origin,
                         const void* resource_context,
                         const content::WebPluginInfo& plugin_info) {
    bool is_available = false;

    // ChromePluginServiceFilter::IsPluginAvailable always runs on the IO
    // thread. Use a RunLoop to ensure this method blocks until it posts back.
    base::RunLoop run_loop;
    content::BrowserThread::PostTaskAndReply(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&ChromePluginServiceFilterTest::IsPluginAvailableOnIOThread,
                   base::Unretained(this), plugin_content_url,
                   main_frame_origin, resource_context, plugin_info,
                   &is_available),
        run_loop.QuitClosure());
    run_loop.Run();

    return is_available;
  }

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SiteEngagementScore::SetParamValuesForTesting();
    // Ensure that the testing profile is registered for creating a PluginPrefs.
    PluginPrefs::GetForTestingProfile(profile());
    PluginFinder::GetInstance();

    filter_ = ChromePluginServiceFilter::GetInstance();
    filter_->RegisterResourceContext(profile(),
                                     profile()->GetResourceContext());
  }

  void IsPluginAvailableOnIOThread(const GURL& plugin_content_url,
                                   const url::Origin& main_frame_origin,
                                   const void* resource_context,
                                   content::WebPluginInfo plugin_info,
                                   bool* is_available) {
    *is_available = filter_->IsPluginAvailable(
        web_contents()->GetRenderProcessHost()->GetID(),
        web_contents()->GetMainFrame()->GetRoutingID(), resource_context,
        plugin_content_url, main_frame_origin, &plugin_info);
  }

  ChromePluginServiceFilter* filter_;
  base::FilePath flash_plugin_path_;
};

TEST_F(ChromePluginServiceFilterTest, FlashAvailableByDefault) {
  content::WebPluginInfo flash_plugin(
      base::ASCIIToUTF16(content::kFlashPluginName), flash_plugin_path_,
      base::ASCIIToUTF16("1"), base::ASCIIToUTF16("The Flash plugin."));
  EXPECT_TRUE(IsPluginAvailable(GURL(), url::Origin(),
                                profile()->GetResourceContext(), flash_plugin));
}

TEST_F(ChromePluginServiceFilterTest, PreferHtmlOverPluginsDefault) {
  content::WebPluginInfo flash_plugin(
      base::ASCIIToUTF16(content::kFlashPluginName), flash_plugin_path_,
      base::ASCIIToUTF16("1"), base::ASCIIToUTF16("The Flash plugin."));
  base::HistogramTester histograms;

  // Activate PreferHtmlOverPlugins.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPreferHtmlOverPlugins);

  // The default content setting should block Flash, as there should be 0
  // engagement.
  GURL url("http://www.google.com");
  url::Origin main_frame_origin(url);
  EXPECT_FALSE(IsPluginAvailable(
      url, main_frame_origin, profile()->GetResourceContext(), flash_plugin));

  histograms.ExpectUniqueSample(
      ChromePluginServiceFilter::kEngagementNoSettingHistogram, 0, 1);

  // Block plugins.
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                CONTENT_SETTING_BLOCK);

  EXPECT_FALSE(IsPluginAvailable(
      url, main_frame_origin, profile()->GetResourceContext(), flash_plugin));

  histograms.ExpectUniqueSample(
      ChromePluginServiceFilter::kEngagementNoSettingHistogram, 0, 1);
  histograms.ExpectUniqueSample(
      ChromePluginServiceFilter::kEngagementSettingBlockedHistogram, 0, 1);

  // Allow plugins.
  map->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                CONTENT_SETTING_ALLOW);

  EXPECT_TRUE(IsPluginAvailable(url, main_frame_origin,
                                profile()->GetResourceContext(), flash_plugin));

  histograms.ExpectUniqueSample(
      ChromePluginServiceFilter::kEngagementNoSettingHistogram, 0, 1);
  histograms.ExpectUniqueSample(
      ChromePluginServiceFilter::kEngagementSettingAllowedHistogram, 0, 1);
  histograms.ExpectUniqueSample(
      ChromePluginServiceFilter::kEngagementSettingBlockedHistogram, 0, 1);

  // Detect important content should block on 0 engagement.
  map->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                CONTENT_SETTING_DETECT_IMPORTANT_CONTENT);

  EXPECT_FALSE(IsPluginAvailable(
      url, main_frame_origin, profile()->GetResourceContext(), flash_plugin));

  histograms.ExpectUniqueSample(
      ChromePluginServiceFilter::kEngagementNoSettingHistogram, 0, 2);
  histograms.ExpectUniqueSample(
      ChromePluginServiceFilter::kEngagementSettingAllowedHistogram, 0, 1);
  histograms.ExpectUniqueSample(
      ChromePluginServiceFilter::kEngagementSettingBlockedHistogram, 0, 1);
}

TEST_F(ChromePluginServiceFilterTest,
       PreferHtmlOverPluginsAllowOrBlockOverrides) {
  content::WebPluginInfo flash_plugin(
      base::ASCIIToUTF16(content::kFlashPluginName), flash_plugin_path_,
      base::ASCIIToUTF16("1"), base::ASCIIToUTF16("The Flash plugin."));
  base::HistogramTester histograms;

  // Activate PreferHtmlOverPlugins.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPreferHtmlOverPlugins);

  // Allow plugins by default.
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                CONTENT_SETTING_ALLOW);

  // This should respect the content setting and be allowed.
  GURL url("http://www.google.com");
  url::Origin main_frame_origin(url);
  EXPECT_TRUE(IsPluginAvailable(url, main_frame_origin,
                                profile()->GetResourceContext(), flash_plugin));

  histograms.ExpectBucketCount(
      ChromePluginServiceFilter::kEngagementSettingAllowedHistogram, 0, 1);

  map->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                CONTENT_SETTING_DETECT_IMPORTANT_CONTENT);

  // This should be blocked due to 0 engagement and a detect content setting.
  EXPECT_FALSE(IsPluginAvailable(
      url, main_frame_origin, profile()->GetResourceContext(), flash_plugin));
  histograms.ExpectBucketCount(
      ChromePluginServiceFilter::kEngagementNoSettingHistogram, 0, 1);

  SiteEngagementService* service = SiteEngagementService::Get(profile());
  service->ResetScoreForURL(url, 10.0);

  // Should still be blocked.
  EXPECT_FALSE(IsPluginAvailable(
      url, main_frame_origin, profile()->GetResourceContext(), flash_plugin));
  histograms.ExpectBucketCount(
      ChromePluginServiceFilter::kEngagementNoSettingHistogram, 10, 1);

  // Reaching 30.0 engagement should allow Flash.
  service->ResetScoreForURL(url, 30.0);
  EXPECT_TRUE(IsPluginAvailable(url, main_frame_origin,
                                profile()->GetResourceContext(), flash_plugin));

  histograms.ExpectBucketCount(
      ChromePluginServiceFilter::kEngagementNoSettingHistogram, 0, 1);
  histograms.ExpectBucketCount(
      ChromePluginServiceFilter::kEngagementNoSettingHistogram, 10, 1);
  histograms.ExpectBucketCount(
      ChromePluginServiceFilter::kEngagementNoSettingHistogram, 30, 1);

  // Blocked content setting should override engagement
  map->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(IsPluginAvailable(
      url, main_frame_origin, profile()->GetResourceContext(), flash_plugin));

  histograms.ExpectBucketCount(
      ChromePluginServiceFilter::kEngagementSettingBlockedHistogram, 30, 1);
}

TEST_F(ChromePluginServiceFilterTest, PreferHtmlOverPluginsCustomEngagement) {
  content::WebPluginInfo flash_plugin(
      base::ASCIIToUTF16(content::kFlashPluginName), flash_plugin_path_,
      base::ASCIIToUTF16("1"), base::ASCIIToUTF16("The Flash plugin."));
  base::HistogramTester histograms;

  // Activate PreferHtmlOverPlugins and set a custom variation value in the
  // feature.
  base::FieldTrialList field_trials_(nullptr);
  base::FieldTrial* trial =
      base::FieldTrialList::CreateFieldTrial(kTrialName, kGroupName);
  std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
  feature_list->RegisterFieldTrialOverride(
      features::kPreferHtmlOverPlugins.name,
      base::FeatureList::OVERRIDE_ENABLE_FEATURE, trial);
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureList(std::move(feature_list));
  EXPECT_EQ(
      base::FeatureList::GetFieldTrial(features::kPreferHtmlOverPlugins),
      trial);

  // Set the custom engagement threshold for Flash.
  std::map<std::string, std::string> params;
  params[PluginsFieldTrial::kSiteEngagementThresholdForFlashKey] = "50";
  ASSERT_TRUE(
      variations::AssociateVariationParams(kTrialName, kGroupName, params));
  std::map<std::string, std::string> actualParams;
  EXPECT_TRUE(variations::GetVariationParamsByFeature(
        features::kPreferHtmlOverPlugins, &actualParams));
  EXPECT_EQ(params, actualParams);

  // Set to detect important content by default.
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                CONTENT_SETTING_DETECT_IMPORTANT_CONTENT);

  // This should be blocked due to 0 engagement.
  GURL url("http://www.google.com");
  url::Origin main_frame_origin(url);
  EXPECT_FALSE(IsPluginAvailable(
      url, main_frame_origin, profile()->GetResourceContext(), flash_plugin));

  // Should still be blocked until engagement reaches 50.
  SiteEngagementService* service = SiteEngagementService::Get(profile());
  service->ResetScoreForURL(url, 0.0);
  EXPECT_FALSE(IsPluginAvailable(
      url, main_frame_origin, profile()->GetResourceContext(), flash_plugin));
  service->ResetScoreForURL(url, 10.0);
  EXPECT_FALSE(IsPluginAvailable(
      url, main_frame_origin, profile()->GetResourceContext(), flash_plugin));
  service->ResetScoreForURL(url, 40.0);
  EXPECT_FALSE(IsPluginAvailable(
      url, main_frame_origin, profile()->GetResourceContext(), flash_plugin));
  service->ResetScoreForURL(url, 60.0);
  EXPECT_TRUE(IsPluginAvailable(url, main_frame_origin,
                                profile()->GetResourceContext(), flash_plugin));

  histograms.ExpectBucketCount(
      ChromePluginServiceFilter::kEngagementNoSettingHistogram, 0, 2);
  histograms.ExpectBucketCount(
      ChromePluginServiceFilter::kEngagementNoSettingHistogram, 10, 1);
  histograms.ExpectBucketCount(
      ChromePluginServiceFilter::kEngagementNoSettingHistogram, 40, 1);
  histograms.ExpectBucketCount(
      ChromePluginServiceFilter::kEngagementNoSettingHistogram, 60, 1);

  variations::testing::ClearAllVariationParams();
}

TEST_F(ChromePluginServiceFilterTest,
       PreferHtmlOverPluginsIncognitoBlockToDetect) {
  Profile* incognito = profile()->GetOffTheRecordProfile();
  filter_->RegisterResourceContext(incognito, incognito->GetResourceContext());

  content::WebPluginInfo flash_plugin(
      base::ASCIIToUTF16(content::kFlashPluginName), flash_plugin_path_,
      base::ASCIIToUTF16("1"), base::ASCIIToUTF16("The Flash plugin."));

  // Activate PreferHtmlOverPlugins.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPreferHtmlOverPlugins);

  // Block plugins in the original profile. This should inherit into incognito.
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                CONTENT_SETTING_BLOCK);

  // We should fail the availablity check in incognito.
  GURL url("http://www.google.com");
  url::Origin main_frame_origin(url);
  EXPECT_FALSE(IsPluginAvailable(
      url, main_frame_origin, incognito->GetResourceContext(), flash_plugin));

  // Add sufficient engagement to allow Flash in the original profile.
  SiteEngagementService* service = SiteEngagementService::Get(profile());
  service->ResetScoreForURL(url, 30.0);

  // We should still fail the engagement check due to the block.
  EXPECT_FALSE(IsPluginAvailable(
      url, main_frame_origin, incognito->GetResourceContext(), flash_plugin));

  // Change to detect important content in the original profile.
  map->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                CONTENT_SETTING_DETECT_IMPORTANT_CONTENT);

  // Ensure we pass the engagement check in the incognito profile (i.e. it falls
  // back to checking engagement from the original profile when nothing is found
  // in the incognito profile).
  EXPECT_TRUE(IsPluginAvailable(url, main_frame_origin,
                                incognito->GetResourceContext(), flash_plugin));
}

TEST_F(ChromePluginServiceFilterTest,
       PreferHtmlOverPluginsIncognitoAllowToDetect) {
  Profile* incognito = profile()->GetOffTheRecordProfile();
  filter_->RegisterResourceContext(incognito, incognito->GetResourceContext());

  content::WebPluginInfo flash_plugin(
      base::ASCIIToUTF16(content::kFlashPluginName), flash_plugin_path_,
      base::ASCIIToUTF16("1"), base::ASCIIToUTF16("The Flash plugin."));

  // Activate PreferHtmlOverPlugins.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPreferHtmlOverPlugins);

  GURL url("http://www.google.com");

  // Allow plugins for this url in the incognito profile.
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(incognito);
  map->SetContentSettingDefaultScope(
      url, url,
      CONTENT_SETTINGS_TYPE_PLUGINS,
      std::string(),
      CONTENT_SETTING_ALLOW);

  // We pass the availablity check in incognito based on the original content
  // setting.
  url::Origin main_frame_origin(url);
  EXPECT_TRUE(IsPluginAvailable(url, main_frame_origin,
                                incognito->GetResourceContext(), flash_plugin));

  map->SetContentSettingDefaultScope(
      url, url,
      CONTENT_SETTINGS_TYPE_PLUGINS,
      std::string(),
      CONTENT_SETTING_DETECT_IMPORTANT_CONTENT);

  // Now we fail the availability check due to the content setting carrying
  // over.
  EXPECT_FALSE(IsPluginAvailable(
      url, main_frame_origin, incognito->GetResourceContext(), flash_plugin));

  // Add sufficient engagement to allow Flash in the incognito profile.
  SiteEngagementService* service = SiteEngagementService::Get(incognito);
  service->ResetScoreForURL(url, 30.0);

  // Ensure we pass the engagement check in the incognito profile.
  EXPECT_TRUE(IsPluginAvailable(url, main_frame_origin,
                                incognito->GetResourceContext(), flash_plugin));
}

TEST_F(ChromePluginServiceFilterTest, ManagedSetting) {
  content::WebPluginInfo flash_plugin(
      base::ASCIIToUTF16(content::kFlashPluginName), flash_plugin_path_,
      base::ASCIIToUTF16("1"), base::ASCIIToUTF16("The Flash plugin."));

  // Activate PreferHtmlOverPlugins.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPreferHtmlOverPlugins);

  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                CONTENT_SETTING_DETECT_IMPORTANT_CONTENT);

  sync_preferences::TestingPrefServiceSyncable* prefs =
      profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedDefaultPluginsSetting,
                        new base::Value(CONTENT_SETTING_ASK));

  SiteEngagementService* service = SiteEngagementService::Get(profile());
  GURL url("http://www.google.com");
  url::Origin main_frame_origin(url);
  NavigateAndCommit(url);

  service->ResetScoreForURL(url, 30.0);
  // Reaching 30.0 engagement would usually allow Flash, but not for enterprise.
  service->ResetScoreForURL(url, 0);
  EXPECT_FALSE(IsPluginAvailable(
      url, main_frame_origin, profile()->GetResourceContext(), flash_plugin));

  // Allow flash temporarily.
  FlashTemporaryPermissionTracker::Get(profile())->FlashEnabledForWebContents(
      web_contents());
  EXPECT_TRUE(IsPluginAvailable(url, main_frame_origin,
                                profile()->GetResourceContext(), flash_plugin));
}
