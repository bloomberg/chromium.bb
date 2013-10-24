// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/message_center_settings_controller.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/notifier_settings.h"

class MessageCenterSettingsControllerTest : public testing::Test {
 protected:
  MessageCenterSettingsControllerTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {};
  virtual ~MessageCenterSettingsControllerTest() {};

  base::FilePath GetProfilePath(const std::string& base_name) {
    return testing_profile_manager_.profile_manager()->user_data_dir()
        .AppendASCII(base_name);
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
  }

  ProfileInfoCache* GetCache() {
    return testing_profile_manager_.profile_info_cache();
  }

  void CreateProfile(const std::string& name) {
    testing_profile_manager_.CreateTestingProfile(name);
  }

  TestingProfileManager testing_profile_manager_;
};

TEST_F(MessageCenterSettingsControllerTest, NotifierGroups) {
  CreateProfile("Profile-1");
  CreateProfile("Profile-2");

  scoped_ptr<MessageCenterSettingsController> controller(
      new MessageCenterSettingsController(GetCache()));

  EXPECT_EQ(controller->GetNotifierGroupCount(), 2u);

  EXPECT_EQ(controller->GetNotifierGroupAt(0).name, UTF8ToUTF16("Profile-1"));
  EXPECT_EQ(controller->GetNotifierGroupAt(0).index, 0u);

  EXPECT_EQ(controller->GetNotifierGroupAt(1).name, UTF8ToUTF16("Profile-2"));
  EXPECT_EQ(controller->GetNotifierGroupAt(1).index, 1u);

  EXPECT_EQ(controller->GetActiveNotifierGroup().name,
            UTF8ToUTF16("Profile-1"));
  EXPECT_EQ(controller->GetActiveNotifierGroup().index, 0u);

  controller->SwitchToNotifierGroup(1);
  EXPECT_EQ(controller->GetActiveNotifierGroup().name,
            UTF8ToUTF16("Profile-2"));
  EXPECT_EQ(controller->GetActiveNotifierGroup().index, 1u);

  controller->SwitchToNotifierGroup(0);
  EXPECT_EQ(controller->GetActiveNotifierGroup().name,
            UTF8ToUTF16("Profile-1"));
}

// TODO(mukai): write a test case to reproduce the actual guest session scenario
// in ChromeOS -- no profiles in the profile_info_cache but GetDefaultProfile
// returns a new one.
