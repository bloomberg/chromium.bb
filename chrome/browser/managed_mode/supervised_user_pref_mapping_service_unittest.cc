// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/prefs/pref_service.h"
#include "chrome/browser/managed_mode/managed_user_constants.h"
#include "chrome/browser/managed_mode/managed_user_shared_settings_service.h"
#include "chrome/browser/managed_mode/managed_user_shared_settings_service_factory.h"
#include "chrome/browser/managed_mode/supervised_user_pref_mapping_service.h"
#include "chrome/browser/managed_mode/supervised_user_pref_mapping_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kFakeManagedUserId[] = "fakeID";

class SupervisedUserPrefMappingServiceTest : public ::testing::Test {
 protected:
  SupervisedUserPrefMappingServiceTest() {
    profile_.GetPrefs()->SetString(prefs::kManagedUserId, kFakeManagedUserId);
    shared_settings_service_ =
        ManagedUserSharedSettingsServiceFactory::GetForBrowserContext(
            &profile_);
    mapping_service_ =
        SupervisedUserPrefMappingServiceFactory::GetForBrowserContext(
            &profile_);
  }
  virtual ~SupervisedUserPrefMappingServiceTest() {}

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    mapping_service_->Init();
  }

  virtual void TearDown() OVERRIDE {
    mapping_service_->Shutdown();
    shared_settings_service_->Shutdown();
  }

  TestingProfile profile_;
  ManagedUserSharedSettingsService* shared_settings_service_;
  SupervisedUserPrefMappingService* mapping_service_;
};

TEST_F(SupervisedUserPrefMappingServiceTest, CheckPrefUpdate) {
  EXPECT_TRUE(shared_settings_service_->GetValue(
                  kFakeManagedUserId, managed_users::kChromeAvatarIndex) ==
              NULL);
  profile_.GetPrefs()->SetInteger(prefs::kProfileAvatarIndex, 4);
  const base::Value* value = shared_settings_service_->GetValue(
      kFakeManagedUserId, managed_users::kChromeAvatarIndex);
  int avatar_index;
  value->GetAsInteger(&avatar_index);
  EXPECT_EQ(avatar_index, 4);
}

TEST_F(SupervisedUserPrefMappingServiceTest, CheckSharedSettingUpdate) {
  EXPECT_EQ(profile_.GetPrefs()->GetInteger(prefs::kProfileAvatarIndex), -1);
  shared_settings_service_->SetValue(kFakeManagedUserId,
                                     managed_users::kChromeAvatarIndex,
                                     base::FundamentalValue(1));
  mapping_service_->OnSharedSettingChanged(kFakeManagedUserId,
                                           managed_users::kChromeAvatarIndex);
  EXPECT_EQ(profile_.GetPrefs()->GetInteger(prefs::kProfileAvatarIndex), 1);
}
