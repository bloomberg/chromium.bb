// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "chrome/browser/search/hotword_service.h"
#include "chrome/browser/search/hotword_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

class HotwordServiceTest : public testing::Test {
 protected:
  HotwordServiceTest() {}
  virtual ~HotwordServiceTest() {}
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
