// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/notifications/desktop_notification_profile_util.h"
#include "chrome/browser/notifications/message_center_settings_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/notifier_settings.h"

#if defined(OS_CHROMEOS)
#include "ash/system/system_notifier.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#endif

class MessageCenterSettingsControllerBaseTest : public testing::Test {
 protected:
  MessageCenterSettingsControllerBaseTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  ~MessageCenterSettingsControllerBaseTest() override{};

  base::FilePath GetProfilePath(const std::string& base_name) {
    return testing_profile_manager_.profile_manager()->user_data_dir()
        .AppendASCII(base_name);
  }

  void SetUp() override { ASSERT_TRUE(testing_profile_manager_.SetUp()); }

  virtual TestingProfile* CreateProfile(const std::string& name) {
    return testing_profile_manager_.CreateTestingProfile(name);
  }

  void CreateController() {
    controller_.reset(new MessageCenterSettingsController(
        *testing_profile_manager_.profile_attributes_storage()));
  }

  void ResetController() {
    controller_.reset();
  }

  MessageCenterSettingsController* controller() { return controller_.get(); }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfileManager testing_profile_manager_;
  std::unique_ptr<MessageCenterSettingsController> controller_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterSettingsControllerBaseTest);
};

#if defined(OS_CHROMEOS)

class MessageCenterSettingsControllerChromeOSTest
    : public MessageCenterSettingsControllerBaseTest {
 protected:
  MessageCenterSettingsControllerChromeOSTest() {}
  ~MessageCenterSettingsControllerChromeOSTest() override {}

  void SetUp() override {
    MessageCenterSettingsControllerBaseTest::SetUp();

    // Initialize the UserManager singleton to a fresh FakeUserManager instance.
    user_manager_enabler_.reset(new chromeos::ScopedUserManagerEnabler(
        new chromeos::FakeChromeUserManager));
  }

  void TearDown() override {
    ResetController();
    MessageCenterSettingsControllerBaseTest::TearDown();
  }

  TestingProfile* CreateProfile(const std::string& name) override {
    TestingProfile* profile =
        MessageCenterSettingsControllerBaseTest::CreateProfile(name);

    GetFakeUserManager()->AddUser(AccountId::FromUserEmail(name));
    GetFakeUserManager()->LoginUser(AccountId::FromUserEmail(name));
    return profile;
  }

  void SwitchActiveUser(const std::string& name) {
    GetFakeUserManager()->SwitchActiveUser(AccountId::FromUserEmail(name));
    controller()->ActiveUserChanged(GetFakeUserManager()->GetActiveUser());
  }

 private:
  chromeos::FakeChromeUserManager* GetFakeUserManager() {
    return static_cast<chromeos::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  std::unique_ptr<chromeos::ScopedUserManagerEnabler> user_manager_enabler_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterSettingsControllerChromeOSTest);
};

typedef MessageCenterSettingsControllerChromeOSTest
    MessageCenterSettingsControllerTest;
#else
typedef MessageCenterSettingsControllerBaseTest
    MessageCenterSettingsControllerTest;
#endif  // OS_CHROMEOS

#if !defined(OS_CHROMEOS)
TEST_F(MessageCenterSettingsControllerTest, NotifierGroups) {
  CreateProfile("Profile-1");
  CreateProfile("Profile-2");
  CreateController();

  EXPECT_EQ(controller()->GetNotifierGroupCount(), 2u);

  EXPECT_EQ(controller()->GetNotifierGroupAt(0).name,
            base::UTF8ToUTF16("Profile-1"));
  EXPECT_EQ(controller()->GetNotifierGroupAt(1).name,
            base::UTF8ToUTF16("Profile-2"));

  EXPECT_EQ(controller()->GetActiveNotifierGroup().name,
            base::UTF8ToUTF16("Profile-1"));

  controller()->SwitchToNotifierGroup(1);
  EXPECT_EQ(controller()->GetActiveNotifierGroup().name,
            base::UTF8ToUTF16("Profile-2"));

  controller()->SwitchToNotifierGroup(0);
  EXPECT_EQ(controller()->GetActiveNotifierGroup().name,
            base::UTF8ToUTF16("Profile-1"));
}
#else   // !defined(OS_CHROMEOS)
TEST_F(MessageCenterSettingsControllerChromeOSTest, NotifierGroups) {
  CreateProfile("Profile-1");
  CreateProfile("Profile-2");
  CreateController();

  EXPECT_EQ(controller()->GetNotifierGroupCount(), 1u);

  EXPECT_EQ(controller()->GetNotifierGroupAt(0).name,
            base::UTF8ToUTF16("Profile-1"));

  SwitchActiveUser("Profile-2");
  EXPECT_EQ(controller()->GetNotifierGroupCount(), 1u);
  EXPECT_EQ(controller()->GetNotifierGroupAt(0).name,
            base::UTF8ToUTF16("Profile-2"));

  SwitchActiveUser("Profile-1");
  EXPECT_EQ(controller()->GetNotifierGroupCount(), 1u);
  EXPECT_EQ(controller()->GetNotifierGroupAt(0).name,
            base::UTF8ToUTF16("Profile-1"));
}
// TODO(mukai): write a test case to reproduce the actual guest session scenario
// in ChromeOS -- no profiles in |profile_attributes_storage_|.
#endif  // !defined(OS_CHROMEOS)

TEST_F(MessageCenterSettingsControllerTest, NotifierSortOrder) {
  TestingProfile* profile = CreateProfile("Profile-1");
  extensions::TestExtensionSystem* test_extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile));
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  ExtensionService* extension_service =
      test_extension_system->CreateExtensionService(
          &command_line, base::FilePath() /* install_directory */,
          false /* autoupdate_enabled*/);

  extensions::ExtensionBuilder foo_app;
  // Foo is an app with name Foo and should appear second.
  const std::string kFooId = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

  // Bar is an app with name Bar and should appear first.
  const std::string kBarId = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

  // Baz is an app with name Baz and should not appear in the notifier list
  // since it doesn't have notifications permission.
  const std::string kBazId = "cccccccccccccccccccccccccccccccc";

  // Baf is a hosted app which should not appear in the notifier list.
  const std::string kBafId = "dddddddddddddddddddddddddddddddd";

  foo_app.SetManifest(
      extensions::DictionaryBuilder()
          .Set("name", "Foo")
          .Set("version", "1.0.0")
          .Set("manifest_version", 2)
          .Set("app", extensions::DictionaryBuilder()
                          .Set("background",
                               extensions::DictionaryBuilder()
                                   .Set("scripts", extensions::ListBuilder()
                                                       .Append("background.js")
                                                       .Build())
                                   .Build())
                          .Build())
          .Set("permissions",
               extensions::ListBuilder().Append("notifications").Build())
          .Build());
  foo_app.SetID(kFooId);
  extension_service->AddExtension(foo_app.Build().get());

  extensions::ExtensionBuilder bar_app;
  bar_app.SetManifest(
      extensions::DictionaryBuilder()
          .Set("name", "Bar")
          .Set("version", "1.0.0")
          .Set("manifest_version", 2)
          .Set("app", extensions::DictionaryBuilder()
                          .Set("background",
                               extensions::DictionaryBuilder()
                                   .Set("scripts", extensions::ListBuilder()
                                                       .Append("background.js")
                                                       .Build())
                                   .Build())
                          .Build())
          .Set("permissions",
               extensions::ListBuilder().Append("notifications").Build())
          .Build());
  bar_app.SetID(kBarId);
  extension_service->AddExtension(bar_app.Build().get());

  extensions::ExtensionBuilder baz_app;
  baz_app.SetManifest(
      extensions::DictionaryBuilder()
          .Set("name", "baz")
          .Set("version", "1.0.0")
          .Set("manifest_version", 2)
          .Set("app", extensions::DictionaryBuilder()
                          .Set("background",
                               extensions::DictionaryBuilder()
                                   .Set("scripts", extensions::ListBuilder()
                                                       .Append("background.js")
                                                       .Build())
                                   .Build())
                          .Build())
          .Build());
  baz_app.SetID(kBazId);
  extension_service->AddExtension(baz_app.Build().get());

  extensions::ExtensionBuilder baf_app;
  baf_app.SetManifest(
      extensions::DictionaryBuilder()
          .Set("name", "baf")
          .Set("version", "1.0.0")
          .Set("manifest_version", 2)
          .Set("app",
               extensions::DictionaryBuilder()
                   .Set("urls", extensions::ListBuilder()
                                    .Append("http://localhost/extensions/"
                                            "hosted_app/main.html")
                                    .Build())
                   .Build())
          .Set("launch",
               extensions::DictionaryBuilder()
                   .Set("urls", extensions::ListBuilder()
                                    .Append("http://localhost/extensions/"
                                            "hosted_app/main.html")
                                    .Build())
                   .Build())
          .Build());

  baf_app.SetID(kBafId);
  extension_service->AddExtension(baf_app.Build().get());
  CreateController();

  std::vector<message_center::Notifier*> notifiers;
  controller()->GetNotifierList(&notifiers);

#if !defined(OS_CHROMEOS)
  EXPECT_EQ(2u, notifiers.size());
#else
  // ChromeOS always adds a system notifier to end of the list.
  EXPECT_EQ(3u, notifiers.size());
  EXPECT_EQ(ash::system_notifier::kNotifierScreenshot,
            notifiers[2]->notifier_id.id);
#endif

  EXPECT_EQ(kBarId, notifiers[0]->notifier_id.id);
  EXPECT_EQ(kFooId, notifiers[1]->notifier_id.id);

  STLDeleteElements(&notifiers);
}

TEST_F(MessageCenterSettingsControllerTest, SetWebPageNotifierEnabled) {
  Profile* profile = CreateProfile("MyProfile");
  CreateController();

  GURL origin("https://example.com/");

  message_center::NotifierId notifier_id(origin);
  message_center::Notifier enabled_notifier(
      notifier_id, base::string16(), true);
  message_center::Notifier disabled_notifier(
      notifier_id, base::string16(), false);

  ContentSetting default_setting =
      HostContentSettingsMapFactory::GetForProfile(profile)
          ->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_NOTIFICATIONS, NULL);
  ASSERT_EQ(CONTENT_SETTING_ASK, default_setting);

  // (1) Enable the permission when the default is to ask (expected to set).
  controller()->SetNotifierEnabled(disabled_notifier, true);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            DesktopNotificationProfileUtil::GetContentSetting(profile, origin));

  // (2) Disable the permission when the default is to ask (expected to clear).
  controller()->SetNotifierEnabled(enabled_notifier, false);
  EXPECT_EQ(CONTENT_SETTING_ASK,
            DesktopNotificationProfileUtil::GetContentSetting(profile, origin));

  // Change the default content setting vaule for notifications to ALLOW.
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                 CONTENT_SETTING_ALLOW);

  // (3) Disable the permission when the default is allowed (expected to set).
  controller()->SetNotifierEnabled(enabled_notifier, false);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            DesktopNotificationProfileUtil::GetContentSetting(profile, origin));

  // (4) Enable the permission when the default is allowed (expected to clear).
  controller()->SetNotifierEnabled(disabled_notifier, true);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            DesktopNotificationProfileUtil::GetContentSetting(profile, origin));

  // Now change the default content setting value to BLOCK.
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
                                 CONTENT_SETTING_BLOCK);

  // (5) Enable the permission when the default is blocked (expected to set).
  controller()->SetNotifierEnabled(disabled_notifier, true);
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            DesktopNotificationProfileUtil::GetContentSetting(profile, origin));

  // (6) Disable the permission when the default is blocked (expected to clear).
  controller()->SetNotifierEnabled(enabled_notifier, false);
  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            DesktopNotificationProfileUtil::GetContentSetting(profile, origin));
}
