// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/notifications/message_center_display_service.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/notifications/stub_notification_platform_bridge.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/desktop_notification_delegate.h"
#include "content/public/common/notification_resources.h"
#include "content/public/common/platform_notification_data.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/features/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission_status.mojom.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/command_line.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/value_builder.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS) && defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

using content::NotificationResources;
using content::PlatformNotificationData;

namespace {

const char kNotificationId[] = "my-notification-id";
const int kNotificationVibrationPattern[] = { 100, 200, 300 };

class MockDesktopNotificationDelegate
    : public content::DesktopNotificationDelegate {
 public:
  MockDesktopNotificationDelegate()
      : displayed_(false),
        clicked_(false) {}

  ~MockDesktopNotificationDelegate() override {}

  // content::DesktopNotificationDelegate implementation.
  void NotificationDisplayed() override { displayed_ = true; }
  void NotificationClosed() override {}
  void NotificationClick() override { clicked_ = true; }

  bool displayed() const { return displayed_; }
  bool clicked() const { return clicked_; }

 private:
  bool displayed_;
  bool clicked_;

  DISALLOW_COPY_AND_ASSIGN(MockDesktopNotificationDelegate);
};

}  // namespace

class PlatformNotificationServiceTest : public testing::Test {
 public:
  void SetUp() override {
    profile_manager_ = base::MakeUnique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("Miguel");
    std::unique_ptr<NotificationUIManager> ui_manager =
        base::MakeUnique<StubNotificationUIManager>();
    std::unique_ptr<NotificationPlatformBridge> notification_bridge =
        base::MakeUnique<StubNotificationPlatformBridge>();

    // TODO(peter): These tests should use the NotificationDisplayService, which
    // will allow the StubNotificationPlatformBridge to be removed.
    TestingBrowserProcess::GetGlobal()->SetNotificationUIManager(
        std::move(ui_manager));
    TestingBrowserProcess::GetGlobal()->SetNotificationPlatformBridge(
        std::move(notification_bridge));
  }

  void TearDown() override {
    profile_manager_.reset();
    TestingBrowserProcess::DeleteInstance();
  }

  void DidGetDisplayedNotifications(
      std::unique_ptr<std::set<std::string>> displayed_notifications,
      bool supports_synchronization) {
    displayed_notifications_ = std::move(displayed_notifications);
  }

 protected:
  // Displays a simple, fake notifications and returns a weak pointer to the
  // delegate receiving events for it (ownership is transferred to the service).
  MockDesktopNotificationDelegate* CreateSimplePageNotification() const {
    return CreateSimplePageNotificationWithCloseClosure(nullptr);
  }

  // Displays a simple, fake notification and returns a weak pointer to the
  // delegate receiving events for it (ownership is transferred to the service).
  // The close closure may be specified if so desired.
  MockDesktopNotificationDelegate* CreateSimplePageNotificationWithCloseClosure(
      base::Closure* close_closure) const {
    PlatformNotificationData notification_data;
    notification_data.title = base::ASCIIToUTF16("My Notification");
    notification_data.body = base::ASCIIToUTF16("Hello, world!");

    MockDesktopNotificationDelegate* delegate =
        new MockDesktopNotificationDelegate();

    service()->DisplayNotification(profile(), kNotificationId,
                                   GURL("https://chrome.com/"),
                                   notification_data, NotificationResources(),
                                   base::WrapUnique(delegate), close_closure);

    return delegate;
  }

  // Returns the Platform Notification Service these unit tests are for.
  PlatformNotificationServiceImpl* service() const {
    return PlatformNotificationServiceImpl::GetInstance();
  }

  // Returns the Profile to be used for these tests.
  Profile* profile() const { return profile_; }

  size_t GetNotificationCount() {
    display_service()->GetDisplayed(base::Bind(
        &PlatformNotificationServiceTest::DidGetDisplayedNotifications,
        base::Unretained(this)));
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(displayed_notifications_.get());
    return displayed_notifications_->size();
  }

  Notification GetDisplayedNotification() {
#if BUILDFLAG(ENABLE_NATIVE_NOTIFICATIONS)
    if (base::FeatureList::IsEnabled(features::kNativeNotifications)) {
      return static_cast<StubNotificationPlatformBridge*>(
                 g_browser_process->notification_platform_bridge())
          ->GetNotificationAt(profile_->GetPath().BaseName().value(), 0);
    } else {
      return static_cast<StubNotificationUIManager*>(
                 g_browser_process->notification_ui_manager())
          ->GetNotificationAt(0);
    }
#else
    return static_cast<StubNotificationUIManager*>(
               g_browser_process->notification_ui_manager())
        ->GetNotificationAt(0);
#endif
  }

 private:
  NotificationDisplayService* display_service() const {
    return NotificationDisplayServiceFactory::GetForProfile(profile_);
  }

  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<std::set<std::string>> displayed_notifications_;
};

TEST_F(PlatformNotificationServiceTest, DisplayPageDisplayedEvent) {
  auto* delegate = CreateSimplePageNotification();

  EXPECT_EQ(1u, GetNotificationCount());
  EXPECT_TRUE(delegate->displayed());
}

TEST_F(PlatformNotificationServiceTest, DisplayPageCloseClosure) {
  base::Closure close_closure;
  CreateSimplePageNotificationWithCloseClosure(&close_closure);

  EXPECT_EQ(1u, GetNotificationCount());

  ASSERT_FALSE(close_closure.is_null());
  close_closure.Run();

  EXPECT_EQ(0u, GetNotificationCount());
  // Note that we cannot verify whether the closed event was called on the
  // delegate given that it'd result in a use-after-free.
}

TEST_F(PlatformNotificationServiceTest, PersistentNotificationDisplay) {
  PlatformNotificationData notification_data;
  notification_data.title = base::ASCIIToUTF16("My notification's title");
  notification_data.body = base::ASCIIToUTF16("Hello, world!");

  service()->DisplayPersistentNotification(
      profile(), kNotificationId, GURL() /* service_worker_scope */,
      GURL("https://chrome.com/"), notification_data, NotificationResources());

  ASSERT_EQ(1u, GetNotificationCount());

  Notification notification = GetDisplayedNotification();
  EXPECT_EQ("https://chrome.com/", notification.origin_url().spec());
  EXPECT_EQ("My notification's title",
      base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ("Hello, world!",
      base::UTF16ToUTF8(notification.message()));

  service()->ClosePersistentNotification(profile(), kNotificationId);
  EXPECT_EQ(0u, GetNotificationCount());
}

TEST_F(PlatformNotificationServiceTest, DisplayPageNotificationMatches) {
  std::vector<int> vibration_pattern(
      kNotificationVibrationPattern,
      kNotificationVibrationPattern + arraysize(kNotificationVibrationPattern));

  PlatformNotificationData notification_data;
  notification_data.title = base::ASCIIToUTF16("My notification's title");
  notification_data.body = base::ASCIIToUTF16("Hello, world!");
  notification_data.vibration_pattern = vibration_pattern;
  notification_data.silent = true;

  MockDesktopNotificationDelegate* delegate
      = new MockDesktopNotificationDelegate();
  service()->DisplayNotification(profile(), kNotificationId,
                                 GURL("https://chrome.com/"), notification_data,
                                 NotificationResources(),
                                 base::WrapUnique(delegate), nullptr);

  ASSERT_EQ(1u, GetNotificationCount());

  Notification notification = GetDisplayedNotification();
  EXPECT_EQ("https://chrome.com/", notification.origin_url().spec());
  EXPECT_EQ("My notification's title",
      base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ("Hello, world!",
      base::UTF16ToUTF8(notification.message()));

  EXPECT_THAT(notification.vibration_pattern(),
      testing::ElementsAreArray(kNotificationVibrationPattern));

  EXPECT_TRUE(notification.silent());
}

TEST_F(PlatformNotificationServiceTest, DisplayPersistentNotificationMatches) {
  std::vector<int> vibration_pattern(
      kNotificationVibrationPattern,
      kNotificationVibrationPattern + arraysize(kNotificationVibrationPattern));

  PlatformNotificationData notification_data;
  notification_data.title = base::ASCIIToUTF16("My notification's title");
  notification_data.body = base::ASCIIToUTF16("Hello, world!");
  notification_data.vibration_pattern = vibration_pattern;
  notification_data.silent = true;
  notification_data.actions.resize(2);
  notification_data.actions[0].type =
      content::PLATFORM_NOTIFICATION_ACTION_TYPE_BUTTON;
  notification_data.actions[0].title = base::ASCIIToUTF16("Button 1");
  notification_data.actions[1].type =
      content::PLATFORM_NOTIFICATION_ACTION_TYPE_TEXT;
  notification_data.actions[1].title = base::ASCIIToUTF16("Button 2");

  NotificationResources notification_resources;
  notification_resources.action_icons.resize(notification_data.actions.size());

  service()->DisplayPersistentNotification(
      profile(), kNotificationId, GURL() /* service_worker_scope */,
      GURL("https://chrome.com/"), notification_data, notification_resources);

  ASSERT_EQ(1u, GetNotificationCount());

  Notification notification = GetDisplayedNotification();
  EXPECT_EQ("https://chrome.com/", notification.origin_url().spec());
  EXPECT_EQ("My notification's title", base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ("Hello, world!", base::UTF16ToUTF8(notification.message()));

  EXPECT_THAT(notification.vibration_pattern(),
              testing::ElementsAreArray(kNotificationVibrationPattern));

  EXPECT_TRUE(notification.silent());

  const auto& buttons = notification.buttons();
  ASSERT_EQ(2u, buttons.size());
  EXPECT_EQ("Button 1", base::UTF16ToUTF8(buttons[0].title));
  EXPECT_EQ(message_center::ButtonType::BUTTON, buttons[0].type);
  EXPECT_EQ("Button 2", base::UTF16ToUTF8(buttons[1].title));
  EXPECT_EQ(message_center::ButtonType::TEXT, buttons[1].type);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)

TEST_F(PlatformNotificationServiceTest, DisplayNameForContextMessage) {
  base::string16 display_name = service()->DisplayNameForContextMessage(
      profile(), GURL("https://chrome.com/"));

  EXPECT_TRUE(display_name.empty());

  // Create a mocked extension.
  scoped_refptr<extensions::Extension> extension =
      extensions::ExtensionBuilder()
          .SetID("honijodknafkokifofgiaalefdiedpko")
          .SetManifest(extensions::DictionaryBuilder()
                           .Set("name", "NotificationTest")
                           .Set("version", "1.0")
                           .Set("manifest_version", 2)
                           .Set("description", "Test Extension")
                           .Build())
          .Build();

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile());
  EXPECT_TRUE(registry->AddEnabled(extension));

  display_name = service()->DisplayNameForContextMessage(
      profile(),
      GURL("chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html"));
  EXPECT_EQ("NotificationTest", base::UTF16ToUTF8(display_name));
}

TEST_F(PlatformNotificationServiceTest, ExtensionPermissionChecks) {
#if defined(OS_CHROMEOS)
  // The ExtensionService on Chrome OS requires these objects to be initialized.
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service;
  chromeos::ScopedTestCrosSettings test_cros_settings;
  chromeos::ScopedTestUserManager test_user_manager;
#endif

  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  extensions::TestExtensionSystem* test_extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile()));

  ExtensionService* extension_service =
      test_extension_system->CreateExtensionService(
          &command_line, base::FilePath() /* install_directory */,
          false /* autoupdate_enabled */);

  // Create a mocked extension that has the notifications API permission.
  scoped_refptr<extensions::Extension> extension =
      extensions::ExtensionBuilder()
          .SetManifest(
              extensions::DictionaryBuilder()
                  .Set("name", "NotificationTest")
                  .Set("version", "1.0")
                  .Set("manifest_version", 2)
                  .Set("description", "Test Extension")
                  .Set(
                      "permissions",
                      extensions::ListBuilder().Append("notifications").Build())
                  .Build())
          .Build();

  // Install the extension on the faked extension service, and verify that it
  // has been added to the extension registry successfully.
  extension_service->AddExtension(extension.get());
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile());

  ASSERT_TRUE(registry->GetExtensionById(
      extension->id(), extensions::ExtensionRegistry::ENABLED));

  const int kFakeRenderProcessId = 42;

  // Mock that the extension is running in a fake render process id.
  extensions::ProcessMap::Get(profile())->Insert(extension->id(),
                                                 kFakeRenderProcessId,
                                                 -1 /* site_instance_id */);

  // Verify that the service indicates that permission has been granted. We only
  // check the UI thread-method for now, as that's the one guarding the behavior
  // in the browser process.
  EXPECT_EQ(blink::mojom::PermissionStatus::GRANTED,
            service()->CheckPermissionOnUIThread(profile(),
                                                 extension->url(),
                                                 kFakeRenderProcessId));
}

TEST_F(PlatformNotificationServiceTest, CreateNotificationFromData) {
  PlatformNotificationData notification_data;
  notification_data.title = base::ASCIIToUTF16("My Notification");
  notification_data.body = base::ASCIIToUTF16("Hello, world!");

  Notification notification = service()->CreateNotificationFromData(
      profile(), GURL() /* service_worker_scope */, GURL("https://chrome.com/"),
      notification_data, NotificationResources(),
      new MockNotificationDelegate("hello"));
  EXPECT_TRUE(notification.context_message().empty());

  // Create a mocked extension.
  scoped_refptr<extensions::Extension> extension =
      extensions::ExtensionBuilder()
          .SetID("honijodknafkokifofgiaalefdiedpko")
          .SetManifest(extensions::DictionaryBuilder()
                           .Set("name", "NotificationTest")
                           .Set("version", "1.0")
                           .Set("manifest_version", 2)
                           .Set("description", "Test Extension")
                           .Build())
          .Build();

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(profile());
  EXPECT_TRUE(registry->AddEnabled(extension));

  notification = service()->CreateNotificationFromData(
      profile(),
      GURL() /* service_worker_scope */,
      GURL("chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html"),
      notification_data, NotificationResources(),
      new MockNotificationDelegate("hello"));
  EXPECT_EQ("NotificationTest",
            base::UTF16ToUTF8(notification.context_message()));
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
