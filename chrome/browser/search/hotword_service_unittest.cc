// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/search/hotword_service.h"
#include "chrome/browser/search/hotword_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

class HotwordServiceTest : public testing::Test {
 protected:
  HotwordServiceTest() : field_trial_list_(NULL) {}
  virtual ~HotwordServiceTest() {}

  void SetApplicationLocale(Profile* profile, const std::string& new_locale) {
#if defined(OS_CHROMEOS)
        // On ChromeOS locale is per-profile.
    profile->GetPrefs()->SetString(prefs::kApplicationLocale, new_locale);
#else
    g_browser_process->SetApplicationLocale(new_locale);
#endif
  }

 private:
  base::FieldTrialList field_trial_list_;
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(HotwordServiceTest, ShouldShowOptInPopup) {
  TestingProfile::Builder profile_builder;
  TestingProfile::Builder otr_profile_builder;
  otr_profile_builder.SetIncognito();
  scoped_ptr<TestingProfile> profile = profile_builder.Build();
  scoped_ptr<TestingProfile> otr_profile = otr_profile_builder.Build();

  // Popup should not be shown for incognito profiles.
  EXPECT_TRUE(otr_profile.get() != NULL);
  EXPECT_FALSE(HotwordServiceFactory::ShouldShowOptInPopup(otr_profile.get()));

  HotwordServiceFactory* hotword_service_factory =
      HotwordServiceFactory::GetInstance();
  HotwordService* hotword_service =
      hotword_service_factory->GetForProfile(profile.get());
  EXPECT_TRUE(hotword_service != NULL);

  // Popup should not be shown if hotword search has been enabled or turned off.
  // Test both paths for accessing whether to do the popup. Just in case.
  profile->GetPrefs()->SetBoolean(prefs::kHotwordSearchEnabled, true);
  EXPECT_FALSE(HotwordServiceFactory::ShouldShowOptInPopup(profile.get()));
  EXPECT_FALSE(hotword_service->ShouldShowOptInPopup());

  profile->GetPrefs()->SetBoolean(prefs::kHotwordSearchEnabled, false);
  EXPECT_FALSE(HotwordServiceFactory::ShouldShowOptInPopup(profile.get()));
  EXPECT_FALSE(hotword_service->ShouldShowOptInPopup());

  // Rest the enabled pref for the next part of the test.
  profile->GetPrefs()->ClearPref(prefs::kHotwordSearchEnabled);

  // Popup should be shown until max number of times are reached.
  for (int i = 0; i < hotword_service->MaxNumberTimesToShowOptInPopup(); i++) {
    EXPECT_TRUE(HotwordServiceFactory::ShouldShowOptInPopup(profile.get()));
    EXPECT_TRUE(hotword_service->ShouldShowOptInPopup());
    hotword_service->ShowOptInPopup();
  }

  // The opt in popup should not be shown if it has already been shown the
  // maximum number of times.
  EXPECT_FALSE(HotwordServiceFactory::ShouldShowOptInPopup(profile.get()));
  EXPECT_FALSE(hotword_service->ShouldShowOptInPopup());

}

TEST_F(HotwordServiceTest, IsHotwordAllowedBadFieldTrial) {
  TestingProfile::Builder profile_builder;
  TestingProfile::Builder otr_profile_builder;
  otr_profile_builder.SetIncognito();
  scoped_ptr<TestingProfile> profile = profile_builder.Build();
  scoped_ptr<TestingProfile> otr_profile = otr_profile_builder.Build();

  HotwordServiceFactory* hotword_service_factory =
      HotwordServiceFactory::GetInstance();

  // Check that the service exists so that a NULL service be ruled out in
  // following tests.
  HotwordService* hotword_service =
      hotword_service_factory->GetForProfile(profile.get());
  EXPECT_TRUE(hotword_service != NULL);

  // When the field trial is empty or Disabled, it should not be allowed.
  std::string group = base::FieldTrialList::FindFullName(
      hotword_internal::kHotwordFieldTrialName);
  EXPECT_TRUE(group.empty());
  EXPECT_FALSE(HotwordServiceFactory::IsHotwordAllowed(profile.get()));

  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
     hotword_internal::kHotwordFieldTrialName,
     hotword_internal::kHotwordFieldTrialDisabledGroupName));
  group = base::FieldTrialList::FindFullName(
      hotword_internal::kHotwordFieldTrialName);
  EXPECT_TRUE(group ==hotword_internal::kHotwordFieldTrialDisabledGroupName);
  EXPECT_FALSE(HotwordServiceFactory::IsHotwordAllowed(profile.get()));

  // Set a valid locale with invalid field trial to be sure it is
  // still false.
  SetApplicationLocale(static_cast<Profile*>(profile.get()), "en");
  EXPECT_FALSE(HotwordServiceFactory::IsHotwordAllowed(profile.get()));

  // Test that incognito returns false as well.
  EXPECT_FALSE(HotwordServiceFactory::IsHotwordAllowed(otr_profile.get()));
}

TEST_F(HotwordServiceTest, IsHotwordAllowedLocale) {
  TestingProfile::Builder profile_builder;
  TestingProfile::Builder otr_profile_builder;
  otr_profile_builder.SetIncognito();
  scoped_ptr<TestingProfile> profile = profile_builder.Build();
  scoped_ptr<TestingProfile> otr_profile = otr_profile_builder.Build();

  HotwordServiceFactory* hotword_service_factory =
      HotwordServiceFactory::GetInstance();

  // Check that the service exists so that a NULL service be ruled out in
  // following tests.
  HotwordService* hotword_service =
      hotword_service_factory->GetForProfile(profile.get());
  EXPECT_TRUE(hotword_service != NULL);

  // Set the field trial to a valid one.
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      hotword_internal::kHotwordFieldTrialName, "Good"));

  // Set the language to an invalid one.
  SetApplicationLocale(static_cast<Profile*>(profile.get()), "non-valid");
  EXPECT_FALSE(HotwordServiceFactory::IsHotwordAllowed(profile.get()));

  // Now with valid locales it should be fine.
  SetApplicationLocale(static_cast<Profile*>(profile.get()), "en");
  EXPECT_TRUE(HotwordServiceFactory::IsHotwordAllowed(profile.get()));
  SetApplicationLocale(static_cast<Profile*>(profile.get()), "en-US");
  EXPECT_TRUE(HotwordServiceFactory::IsHotwordAllowed(profile.get()));
  SetApplicationLocale(static_cast<Profile*>(profile.get()), "en_us");
  EXPECT_TRUE(HotwordServiceFactory::IsHotwordAllowed(profile.get()));

  // Test that incognito even with a valid locale and valid field trial
  // still returns false.
  SetApplicationLocale(static_cast<Profile*>(otr_profile.get()), "en");
  EXPECT_FALSE(HotwordServiceFactory::IsHotwordAllowed(otr_profile.get()));
}

TEST_F(HotwordServiceTest, AudioLoggingPrefSetCorrectly) {
  TestingProfile::Builder profile_builder;
  scoped_ptr<TestingProfile> profile = profile_builder.Build();

  HotwordServiceFactory* hotword_service_factory =
      HotwordServiceFactory::GetInstance();
  HotwordService* hotword_service =
      hotword_service_factory->GetForProfile(profile.get());
  EXPECT_TRUE(hotword_service != NULL);

  // If it's a fresh profile, although the default value is true,
  // it should return false if the preference has never been set.
  EXPECT_FALSE(hotword_service->IsOptedIntoAudioLogging());
}
