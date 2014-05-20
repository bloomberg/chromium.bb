// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
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

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/fake_user_manager.h"
#endif

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

  virtual void CreateProfile(const std::string& name) {
    testing_profile_manager_.CreateTestingProfile(name);
  }

  void CreateController() {
    controller_.reset(new MessageCenterSettingsController(
        testing_profile_manager_.profile_info_cache()));
  }

  void ResetController() {
    controller_.reset();
  }

  MessageCenterSettingsController* controller() { return controller_.get(); }

 private:
  TestingProfileManager testing_profile_manager_;
  scoped_ptr<MessageCenterSettingsController> controller_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterSettingsControllerTest);
};

#if defined(OS_CHROMEOS)
// This value should be same as the one in fake_user_manager.cc
static const char kUserIdHashSuffix[] = "-hash";

class MessageCenterSettingsControllerChromeOSTest
    : public MessageCenterSettingsControllerTest {
 protected:
  MessageCenterSettingsControllerChromeOSTest() {}
  virtual ~MessageCenterSettingsControllerChromeOSTest() {}

  virtual void SetUp() OVERRIDE {
    MessageCenterSettingsControllerTest::SetUp();

    // Initialize the UserManager singleton to a fresh FakeUserManager instance.
    user_manager_enabler_.reset(
        new chromeos::ScopedUserManagerEnabler(new chromeos::FakeUserManager));
  }

  virtual void TearDown() OVERRIDE {
    ResetController();
    MessageCenterSettingsControllerTest::TearDown();
  }

  virtual void CreateProfile(const std::string& name) OVERRIDE {
    MessageCenterSettingsControllerTest::CreateProfile(name);

    GetFakeUserManager()->AddUser(name);
    GetFakeUserManager()->UserLoggedIn(name, name + kUserIdHashSuffix, false);
  }

  void SwitchActiveUser(const std::string& name) {
    GetFakeUserManager()->SwitchActiveUser(name);
    controller()->ActiveUserChanged(GetFakeUserManager()->GetActiveUser());
  }

 private:
  chromeos::FakeUserManager* GetFakeUserManager() {
    return static_cast<chromeos::FakeUserManager*>(
        chromeos::UserManager::Get());
  }

  scoped_ptr<chromeos::ScopedUserManagerEnabler> user_manager_enabler_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterSettingsControllerChromeOSTest);
};
#endif  // OS_CHROMEOS

#if !defined(OS_CHROMEOS)
TEST_F(MessageCenterSettingsControllerTest, NotifierGroups) {
  CreateProfile("Profile-1");
  CreateProfile("Profile-2");
  CreateController();

  EXPECT_EQ(controller()->GetNotifierGroupCount(), 2u);

  EXPECT_EQ(controller()->GetNotifierGroupAt(0).name,
            base::UTF8ToUTF16("Profile-1"));
  EXPECT_EQ(controller()->GetNotifierGroupAt(0).index, 0u);

  EXPECT_EQ(controller()->GetNotifierGroupAt(1).name,
            base::UTF8ToUTF16("Profile-2"));
  EXPECT_EQ(controller()->GetNotifierGroupAt(1).index, 1u);

  EXPECT_EQ(controller()->GetActiveNotifierGroup().name,
            base::UTF8ToUTF16("Profile-1"));
  EXPECT_EQ(controller()->GetActiveNotifierGroup().index, 0u);

  controller()->SwitchToNotifierGroup(1);
  EXPECT_EQ(controller()->GetActiveNotifierGroup().name,
            base::UTF8ToUTF16("Profile-2"));
  EXPECT_EQ(controller()->GetActiveNotifierGroup().index, 1u);

  controller()->SwitchToNotifierGroup(0);
  EXPECT_EQ(controller()->GetActiveNotifierGroup().name,
            base::UTF8ToUTF16("Profile-1"));
}
#else
TEST_F(MessageCenterSettingsControllerChromeOSTest, NotifierGroups) {
  CreateProfile("Profile-1");
  CreateProfile("Profile-2");
  CreateController();

  EXPECT_EQ(controller()->GetNotifierGroupCount(), 1u);

  EXPECT_EQ(controller()->GetNotifierGroupAt(0).name,
            base::UTF8ToUTF16("Profile-1"));
  EXPECT_EQ(controller()->GetNotifierGroupAt(0).index, 0u);

  SwitchActiveUser("Profile-2");
  EXPECT_EQ(controller()->GetNotifierGroupCount(), 1u);
  EXPECT_EQ(controller()->GetNotifierGroupAt(0).name,
            base::UTF8ToUTF16("Profile-2"));
  EXPECT_EQ(controller()->GetNotifierGroupAt(0).index, 1u);

  SwitchActiveUser("Profile-1");
  EXPECT_EQ(controller()->GetNotifierGroupCount(), 1u);
  EXPECT_EQ(controller()->GetNotifierGroupAt(0).name,
            base::UTF8ToUTF16("Profile-1"));
  EXPECT_EQ(controller()->GetNotifierGroupAt(0).index, 0u);
}
#endif

// TODO(mukai): write a test case to reproduce the actual guest session scenario
// in ChromeOS -- no profiles in the profile_info_cache.
