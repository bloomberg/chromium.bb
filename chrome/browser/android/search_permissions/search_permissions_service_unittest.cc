// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/search_permissions/search_permissions_service.h"

#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/search_permissions/search_geolocation_disclosure_tab_helper.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker.h"
#include "chrome/browser/permissions/permission_result.h"
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

const char kIsGoogleSearchEngineKey[] = "is_google_search_engine";
const char kDSESettingKey[] = "dse_setting";

url::Origin ToOrigin(const char* url) {
  return url::Origin::Create(GURL(url));
}

// The test delegate is used to mock out search-engine related functionality.
class TestSearchEngineDelegate
    : public SearchPermissionsService::SearchEngineDelegate {
 public:
  base::string16 GetDSEName() override {
    if (dse_origin_.host().find("google") != std::string::npos)
      return base::ASCIIToUTF16("Google");

    return base::ASCIIToUTF16("Example");
  }

  url::Origin GetDSEOrigin() override { return dse_origin_; }

  void SetDSEChangedCallback(const base::Closure& callback) override {
    dse_changed_callback_ = callback;
  }

  void SetDSEOrigin(const std::string& dse_origin) {
    dse_origin_ = url::Origin::Create(GURL(dse_origin));
    dse_changed_callback_.Run();
  }

 private:
  url::Origin dse_origin_;
  base::Closure dse_changed_callback_;
};

}  // namespace

class SearchPermissionsServiceTest : public testing::Test {
 public:
  void SetUp() override {
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

  SearchPermissionsService* GetService() {
    return SearchPermissionsService::Factory::GetForBrowserContext(profile());
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
      profile()->GetPrefs()->ClearPref(prefs::kDSEGeolocationSetting);

    GetService()->InitializeDSEGeolocationSettingIfNeeded();
  }

  // Simulate setting the old preference to test migration.
  void SetOldPreference(bool is_google, bool setting) {
    base::DictionaryValue dict;
    dict.SetBoolean(kIsGoogleSearchEngineKey, is_google);
    dict.SetBoolean(kDSESettingKey, setting);
    profile()->GetPrefs()->Set(prefs::kGoogleDSEGeolocationSettingDeprecated,
                               dict);
  }

 private:
  std::unique_ptr<TestingProfile> profile_;
  content::TestBrowserThreadBundle thread_bundle_;

  // This is owned by the SearchPermissionsService which is owned by the
  // profile.
  TestSearchEngineDelegate* test_delegate_;
};

TEST_F(SearchPermissionsServiceTest, Initialization) {
  test_delegate()->SetDSEOrigin(kGoogleURL);

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

  // For non-Google search engine, the setting should also be used.
  test_delegate()->SetDSEOrigin(kExampleURL);
  SetContentSetting(kExampleURL, CONTENT_SETTING_ALLOW);
  ReinitializeService(true /* clear_pref */);
  EXPECT_TRUE(GetService()->UseDSEGeolocationSetting(ToOrigin(kExampleURL)));
}

TEST_F(SearchPermissionsServiceTest, Migration) {
  // First test migrating when the search engine is Google, and the setting is
  // on.
  test_delegate()->SetDSEOrigin(kGoogleURL);
  SearchGeolocationDisclosureTabHelper::FakeShowingDisclosureForTests(
      profile());
  SetOldPreference(true /* is_google */, true /* setting */);
  ReinitializeService(true /* clear_pref */);

  // The setting should be true, and the disclosure should not be reset as it
  // has been shown for Google, and the current search engine is Google.
  EXPECT_TRUE(GetService()->GetDSEGeolocationSetting());
  EXPECT_FALSE(SearchGeolocationDisclosureTabHelper::IsDisclosureResetForTests(
      profile()));

  // Now test that migration won't happen again.
  GetService()->SetDSEGeolocationSetting(false);
  ReinitializeService(false /* clear_pref */);
  EXPECT_FALSE(GetService()->GetDSEGeolocationSetting());

  // Secondly, test migrating when the search engine isn't Google, and the
  // setting is on.
  test_delegate()->SetDSEOrigin(kExampleURL);
  SetOldPreference(false /* is_google */, true /* setting */);
  ReinitializeService(true /* clear_pref */);

  // The setting should be true, and the disclosure should be reset as it would
  // have only been shown for Google.
  EXPECT_TRUE(GetService()->GetDSEGeolocationSetting());
  EXPECT_TRUE(SearchGeolocationDisclosureTabHelper::IsDisclosureResetForTests(
      profile()));

  // Last, test migrating when the search engine isn't Google, and the setting
  // is off.
  SearchGeolocationDisclosureTabHelper::FakeShowingDisclosureForTests(
      profile());
  SetOldPreference(false /* is_google */, false /* setting */);
  ReinitializeService(true /* clear_pref */);

  // The setting should be false, and the disclosure should not be reset as the
  // setting is off.
  EXPECT_FALSE(GetService()->GetDSEGeolocationSetting());
  EXPECT_FALSE(SearchGeolocationDisclosureTabHelper::IsDisclosureResetForTests(
      profile()));

  // Note there isn't any need to test when the search engine was Google and the
  // setting was off, as the code paths have been tested by the above cases.
}

TEST_F(SearchPermissionsServiceTest, OffTheRecord) {
  // Service isn't constructed for an OTR profile.
  Profile* otr_profile = profile()->GetOffTheRecordProfile();
  SearchPermissionsService* service =
      SearchPermissionsService::Factory::GetForBrowserContext(otr_profile);
  EXPECT_EQ(nullptr, service);
}

TEST_F(SearchPermissionsServiceTest, UseDSEGeolocationSetting) {
  // True for origin that matches the CCTLD and meets all requirements.
  test_delegate()->SetDSEOrigin(kGoogleURL);
  EXPECT_TRUE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleURL)));

  // False for different origin.
  EXPECT_FALSE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleAusURL)));

  // False for http origin.
  test_delegate()->SetDSEOrigin(kGoogleHTTPURL);
  EXPECT_FALSE(
      GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleHTTPURL)));

  // False if the content setting is enterprise ask.
  test_delegate()->SetDSEOrigin(kGoogleURL);
  profile()->GetTestingPrefService()->SetManagedPref(
      prefs::kManagedDefaultGeolocationSetting,
      base::MakeUnique<base::Value>(CONTENT_SETTING_ASK));
  EXPECT_FALSE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleURL)));
}

TEST_F(SearchPermissionsServiceTest, GetDSEGeolocationSetting) {
  test_delegate()->SetDSEOrigin(kGoogleURL);

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

TEST_F(SearchPermissionsServiceTest, SetDSEGeolocationSetting) {
  test_delegate()->SetDSEOrigin(kGoogleURL);

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
      base::MakeUnique<base::Value>(CONTENT_SETTING_ASK));
  EXPECT_TRUE(GetService()->GetDSEGeolocationSetting());
  GetService()->SetDSEGeolocationSetting(false);
  EXPECT_TRUE(GetService()->GetDSEGeolocationSetting());
}

TEST_F(SearchPermissionsServiceTest, DSEChanges) {
  test_delegate()->SetDSEOrigin(kGoogleURL);
  EXPECT_TRUE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleURL)));
  EXPECT_TRUE(GetService()->GetDSEGeolocationSetting());

  // Change to google.com.au, setting should remain the same.
  test_delegate()->SetDSEOrigin(kGoogleAusURL);
  EXPECT_FALSE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleURL)));
  EXPECT_TRUE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleAusURL)));
  EXPECT_TRUE(GetService()->GetDSEGeolocationSetting());

  // Set the content setting for google.com to block. When we change back to
  // google.com, the setting should be set to false.
  SetContentSetting(kGoogleURL, CONTENT_SETTING_BLOCK);
  test_delegate()->SetDSEOrigin(kGoogleURL);
  EXPECT_TRUE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleURL)));
  EXPECT_FALSE(GetService()->GetDSEGeolocationSetting());

  // Now set the content setting for google.com.au to ALLOW. When we change to
  // google.com.au, its content setting should be reset and the setting should
  // still be false.
  SetContentSetting(kGoogleAusURL, CONTENT_SETTING_ALLOW);
  test_delegate()->SetDSEOrigin(kGoogleAusURL);
  EXPECT_TRUE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleAusURL)));
  EXPECT_FALSE(GetService()->GetDSEGeolocationSetting());
  EXPECT_EQ(CONTENT_SETTING_ASK, GetContentSetting(kGoogleAusURL));

  // Now set to a non-google search. The setting should still be used, but only
  // for the non-google search.
  test_delegate()->SetDSEOrigin(kExampleURL);
  EXPECT_FALSE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleAusURL)));
  EXPECT_TRUE(GetService()->UseDSEGeolocationSetting(ToOrigin(kExampleURL)));

  // Go back to google.com.au. The setting should still be false because that's
  // what it last was.
  test_delegate()->SetDSEOrigin(kGoogleAusURL);
  EXPECT_TRUE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleAusURL)));
  EXPECT_FALSE(GetService()->GetDSEGeolocationSetting());
}

TEST_F(SearchPermissionsServiceTest, DSEChangesAndDisclosure) {
  test_delegate()->SetDSEOrigin(kGoogleURL);
  SearchGeolocationDisclosureTabHelper::FakeShowingDisclosureForTests(
      profile());
  EXPECT_TRUE(GetService()->UseDSEGeolocationSetting(ToOrigin(kGoogleURL)));
  EXPECT_TRUE(GetService()->GetDSEGeolocationSetting());

  // Change to google.com.au. The disclosure should not be reset.
  test_delegate()->SetDSEOrigin(kGoogleAusURL);
  EXPECT_FALSE(SearchGeolocationDisclosureTabHelper::IsDisclosureResetForTests(
      profile()));

  // Now set to a non-google search. The disclosure should be reset.
  test_delegate()->SetDSEOrigin(kExampleURL);
  EXPECT_TRUE(SearchGeolocationDisclosureTabHelper::IsDisclosureResetForTests(
      profile()));
  SearchGeolocationDisclosureTabHelper::FakeShowingDisclosureForTests(
      profile());

  // Go back to google.com.au. The disclosure should again be reset.
  test_delegate()->SetDSEOrigin(kGoogleAusURL);
  EXPECT_TRUE(SearchGeolocationDisclosureTabHelper::IsDisclosureResetForTests(
      profile()));
}

TEST_F(SearchPermissionsServiceTest, Embargo) {
  test_delegate()->SetDSEOrigin(kGoogleURL);

  // Place another origin under embargo.
  GURL google_aus_url(kGoogleAusURL);
  PermissionDecisionAutoBlocker* auto_blocker =
      PermissionDecisionAutoBlocker::GetForProfile(profile());
  auto_blocker->RecordDismissAndEmbargo(google_aus_url,
                                        CONTENT_SETTINGS_TYPE_GEOLOCATION);
  auto_blocker->RecordDismissAndEmbargo(google_aus_url,
                                        CONTENT_SETTINGS_TYPE_GEOLOCATION);
  auto_blocker->RecordDismissAndEmbargo(google_aus_url,
                                        CONTENT_SETTINGS_TYPE_GEOLOCATION);
  PermissionResult result = auto_blocker->GetEmbargoResult(
      GURL(kGoogleAusURL), CONTENT_SETTINGS_TYPE_GEOLOCATION);
  EXPECT_EQ(result.source, PermissionStatusSource::MULTIPLE_DISMISSALS);
  EXPECT_EQ(result.content_setting, CONTENT_SETTING_BLOCK);

  // Now change the DSE to this origin and make sure the embargo is cleared.
  test_delegate()->SetDSEOrigin(kGoogleAusURL);
  result = auto_blocker->GetEmbargoResult(GURL(kGoogleAusURL),
                                          CONTENT_SETTINGS_TYPE_GEOLOCATION);
  EXPECT_EQ(result.source, PermissionStatusSource::UNSPECIFIED);
  EXPECT_EQ(result.content_setting, CONTENT_SETTING_ASK);
}
