// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/search_geolocation/search_geolocation_service.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace {

const char kGoogleURL[] = "https://www.google.com";
const char kGoogleAusURL[] = "https://www.google.com.au";
const char kGoogleHTTPURL[] = "http://www.google.com";
const char kExampleURL[] = "https://www.example.com";

url::Origin ToOrigin(const char* url) {
  return url::Origin(GURL(url));
}

// The test delegate is used to mock out search-engine related functionality.
class TestSearchEngineDelegate
    : public SearchGeolocationService::SearchEngineDelegate {
 public:
  bool IsDSEGoogle() override {
    // A rough heuristic that is good enough for this test.
    return dse_cctld_.host().find("google.com") != std::string::npos;
  }

  url::Origin GetGoogleDSECCTLD() override {
    if (!IsDSEGoogle())
      return url::Origin();

    return dse_cctld_;
  }

  void SetDSEChangedCallback(const base::Closure& callback) override {
    dse_changed_callback_ = callback;
  }

  void SetDSECCTLD(const std::string& dse_cctld) {
    dse_cctld_ = url::Origin(GURL(dse_cctld));
    dse_changed_callback_.Run();
  }

 private:
  url::Origin dse_cctld_;
  base::Closure dse_changed_callback_;
};

}  // namespace

class SearchGeolocationServiceTest : public testing::Test {
 public:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kConsistentOmniboxGeolocation);

    profile_.reset(new TestingProfile);

    auto test_delegate = base::MakeUnique<TestSearchEngineDelegate>();
    test_delegate_ = test_delegate.get();
    GetService()->SetSearchEngineDelegateForTest(std::move(test_delegate));
  }

  void TearDown() override {
    test_delegate_ = nullptr;
    profile_.reset();
  }

  TestingProfile* profile() { return profile_.get(); }

  TestSearchEngineDelegate* test_delegate() { return test_delegate_; }

  SearchGeolocationService* GetService() {
    return SearchGeolocationService::Factory::GetForBrowserContext(profile());
  }

  void SetContentSetting(const std::string& origin_string,
                         ContentSetting setting) {
    GURL url(origin_string);
    HostContentSettingsMapFactory::GetForProfile(profile())
        ->SetContentSettingDefaultScope(url, url,
                                        CONTENT_SETTINGS_TYPE_GEOLOCATION,
                                        std::string(), setting);
  }

  ContentSetting GetContentSetting(const std::string& origin_string) {
    GURL url(origin_string);
    return HostContentSettingsMapFactory::GetForProfile(profile())
        ->GetContentSetting(url, url, CONTENT_SETTINGS_TYPE_GEOLOCATION,
                            std::string());
  }

  // Simulates the initialization that happens when recreating the service. If
  // |clear_pref| is true, then it simulates the first time the service is ever
  // created.
  void ReinitializeService(bool clear_pref) {
    if (clear_pref)
      profile()->GetPrefs()->ClearPref(prefs::kGoogleDSEGeolocationSetting);

    GetService()->InitializeDSEGeolocationSettingIfNeeded();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  content::TestBrowserThreadBundle thread_bundle_;

  // This is owned by the SearchGeolocationService which is owned by the
  // profile.
  TestSearchEngineDelegate* test_delegate_;
};

TEST_F(SearchGeolocationServiceTest, Initialization) {
  test_delegate()->SetDSECCTLD(kGoogleURL);

  // DSE setting initialized to true if the content setting is ALLOW.
  SetContentSetting(kGoogleURL, CONTENT_SETTING_ALLOW);
  ReinitializeService(true /* clear_pref */);
  EXPECT_TRUE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleURL)));
  EXPECT_TRUE(GetService()->GetDSEGeolocationSetting());

  // DSE setting initialized to true if the content setting is ASK.
  SetContentSetting(kGoogleURL, CONTENT_SETTING_ASK);
  ReinitializeService(true /* clear_pref */);
  EXPECT_TRUE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleURL)));
  EXPECT_TRUE(GetService()->GetDSEGeolocationSetting());

  // DSE setting initialized to false if the content setting is BLOCK.
  SetContentSetting(kGoogleURL, CONTENT_SETTING_BLOCK);
  ReinitializeService(true /* clear_pref */);
  EXPECT_TRUE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleURL)));
  EXPECT_FALSE(GetService()->GetDSEGeolocationSetting());

  // Nothing happens if the pref is already set when the service is initialized.
  SetContentSetting(kGoogleURL, CONTENT_SETTING_ASK);
  ReinitializeService(false /* clear_pref */);
  EXPECT_TRUE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleURL)));
  EXPECT_FALSE(GetService()->GetDSEGeolocationSetting());

  // For non-Google search engine, the setting may be initialized to true, but
  // it won't be used.
  test_delegate()->SetDSECCTLD(kExampleURL);
  SetContentSetting(kExampleURL, CONTENT_SETTING_ALLOW);
  ReinitializeService(true /* clear_pref */);
  EXPECT_FALSE(GetService()->UseDSEGeolocationSetting(ToOrigin(kExampleURL)));
}

TEST_F(SearchGeolocationServiceTest, OffTheRecord) {
  // Service isn't constructed for an OTR profile.
  Profile* otr_profile = profile()->GetOffTheRecordProfile();
  SearchGeolocationService* service =
      SearchGeolocationService::Factory::GetForBrowserContext(otr_profile);
  EXPECT_EQ(nullptr, service);
}

TEST_F(SearchGeolocationServiceTest, UseDSEGeolocationSetting) {
  // True for origin that matches the CCTLD and meets all requirements.
  test_delegate()->SetDSECCTLD(kGoogleURL);
  EXPECT_TRUE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleURL)));

  // False for different origin.
  EXPECT_FALSE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleAusURL)));

  // False for http origin.
  test_delegate()->SetDSECCTLD(kGoogleHTTPURL);
  EXPECT_FALSE(
      GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleHTTPURL)));

  // False if the feature is disabled.
  test_delegate()->SetDSECCTLD(kGoogleURL);
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(
        features::kConsistentOmniboxGeolocation);
    EXPECT_FALSE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleURL)));
  }

  // False if the content setting is enterprise ask.
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kManagedDefaultGeolocationSetting,
      new base::Value(CONTENT_SETTING_ASK));
  EXPECT_FALSE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleURL)));
}

TEST_F(SearchGeolocationServiceTest, GetDSEGeolocationSetting) {
  test_delegate()->SetDSECCTLD(kGoogleURL);

  // The case where the pref is set to true.
  GetService()->SetDSEGeolocationSetting(true);
  EXPECT_TRUE(GetService()->GetDSEGeolocationSetting());

  // Make the content setting conflict. Check that it gets made consistent
  // again.
  SetContentSetting(kGoogleURL, CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(GetService()->GetDSEGeolocationSetting());

  // The case where the pref is set to false.
  SetContentSetting(kGoogleURL, CONTENT_SETTING_DEFAULT);
  GetService()->SetDSEGeolocationSetting(false);
  EXPECT_FALSE(GetService()->GetDSEGeolocationSetting());

  // Make the content setting conflict. Check that it gets made consistent
  // again.
  SetContentSetting(kGoogleURL, CONTENT_SETTING_ALLOW);
  EXPECT_TRUE(GetService()->GetDSEGeolocationSetting());
}

TEST_F(SearchGeolocationServiceTest, SetDSEGeolocationSetting) {
  test_delegate()->SetDSECCTLD(kGoogleURL);

  GetService()->SetDSEGeolocationSetting(true);
  EXPECT_TRUE(GetService()->GetDSEGeolocationSetting());

  GetService()->SetDSEGeolocationSetting(false);
  EXPECT_FALSE(GetService()->GetDSEGeolocationSetting());

  // Check that the content setting is always reset when setting the DSE
  // setting.
  SetContentSetting(kGoogleURL, CONTENT_SETTING_ALLOW);
  GetService()->SetDSEGeolocationSetting(true);
  EXPECT_EQ(CONTENT_SETTING_ASK, GetContentSetting(kGoogleURL));

  SetContentSetting(kGoogleURL, CONTENT_SETTING_BLOCK);
  GetService()->SetDSEGeolocationSetting(false);
  EXPECT_EQ(CONTENT_SETTING_ASK, GetContentSetting(kGoogleURL));

  // Check that the pref doesn't change if it's not user settable.
  GetService()->SetDSEGeolocationSetting(true);
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kManagedDefaultGeolocationSetting,
      new base::Value(CONTENT_SETTING_ASK));
  EXPECT_TRUE(GetService()->GetDSEGeolocationSetting());
  GetService()->SetDSEGeolocationSetting(false);
  EXPECT_TRUE(GetService()->GetDSEGeolocationSetting());
}

TEST_F(SearchGeolocationServiceTest, DSEChanges) {
  test_delegate()->SetDSECCTLD(kGoogleURL);
  EXPECT_TRUE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleURL)));
  EXPECT_TRUE(GetService()->GetDSEGeolocationSetting());

  // Change to google.com.au, setting should remain the same.
  test_delegate()->SetDSECCTLD(kGoogleAusURL);
  EXPECT_FALSE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleURL)));
  EXPECT_TRUE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleAusURL)));
  EXPECT_TRUE(GetService()->GetDSEGeolocationSetting());

  // Set the content setting for google.com to block. When we change back to
  // google.com, the setting should be set to false.
  SetContentSetting(kGoogleURL, CONTENT_SETTING_BLOCK);
  test_delegate()->SetDSECCTLD(kGoogleURL);
  EXPECT_TRUE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleURL)));
  EXPECT_FALSE(GetService()->GetDSEGeolocationSetting());

  // Now set the content setting for google.com.au to ALLOW. When we change to
  // google.com.au, its content setting should be reset and the setting should
  // still be false.
  SetContentSetting(kGoogleAusURL, CONTENT_SETTING_ALLOW);
  test_delegate()->SetDSECCTLD(kGoogleAusURL);
  EXPECT_TRUE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleAusURL)));
  EXPECT_FALSE(GetService()->GetDSEGeolocationSetting());
  EXPECT_EQ(CONTENT_SETTING_ASK, GetContentSetting(kGoogleAusURL));

  // Now set to a non-google search. The setting should never be used.
  test_delegate()->SetDSECCTLD(kExampleURL);
  EXPECT_FALSE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleAusURL)));
  EXPECT_FALSE(GetService()->UseDSEGeolocationSetting(ToOrigin(kExampleURL)));

  // Go back to google.com.au. The setting should still be false because that's
  // what it last was.
  test_delegate()->SetDSECCTLD(kGoogleAusURL);
  EXPECT_TRUE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleAusURL)));
  EXPECT_FALSE(GetService()->GetDSEGeolocationSetting());
}
