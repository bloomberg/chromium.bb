// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/supervised_user/legacy/supervised_user_pref_mapping_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_pref_mapping_service_factory.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kFakeSupervisedUserId[] = "fakeID";

class SupervisedUserPrefMappingServiceTest : public ::testing::Test {
 protected:
  SupervisedUserPrefMappingServiceTest() {
    profile_.GetPrefs()->SetString(prefs::kSupervisedUserId,
                                   kFakeSupervisedUserId);
    shared_settings_service_ =
        SupervisedUserSharedSettingsServiceFactory::GetForBrowserContext(
            &profile_);
    mapping_service_ =
        SupervisedUserPrefMappingServiceFactory::GetForBrowserContext(
            &profile_);
  }
  ~SupervisedUserPrefMappingServiceTest() override {}

  // testing::Test overrides:
  void SetUp() override { mapping_service_->Init(); }

  void TearDown() override {
    mapping_service_->Shutdown();
    shared_settings_service_->Shutdown();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  SupervisedUserSharedSettingsService* shared_settings_service_;
  SupervisedUserPrefMappingService* mapping_service_;
};

TEST_F(SupervisedUserPrefMappingServiceTest, CheckPrefUpdate) {
  EXPECT_TRUE(shared_settings_service_->GetValue(
                  kFakeSupervisedUserId,
                  supervised_users::kChromeAvatarIndex) == NULL);
  profile_.GetPrefs()->SetInteger(prefs::kProfileAvatarIndex, 4);
  const base::Value* value = shared_settings_service_->GetValue(
      kFakeSupervisedUserId, supervised_users::kChromeAvatarIndex);
  int avatar_index;
  value->GetAsInteger(&avatar_index);
  EXPECT_EQ(avatar_index, 4);
}

TEST_F(SupervisedUserPrefMappingServiceTest, CheckSharedSettingUpdate) {
  EXPECT_EQ(profile_.GetPrefs()->GetInteger(prefs::kProfileAvatarIndex), -1);
  shared_settings_service_->SetValue(kFakeSupervisedUserId,
                                     supervised_users::kChromeAvatarIndex,
                                     base::Value(1));
  mapping_service_->OnSharedSettingChanged(
      kFakeSupervisedUserId,
      supervised_users::kChromeAvatarIndex);
  EXPECT_EQ(profile_.GetPrefs()->GetInteger(prefs::kProfileAvatarIndex), 1);
}
