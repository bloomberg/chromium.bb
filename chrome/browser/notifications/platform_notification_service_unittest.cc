// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/metrics/mock_notification_metrics_logger.h"
#include "chrome/browser/notifications/metrics/notification_metrics_logger_factory.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/permission_type.h"
#include "content/public/common/notification_resources.h"
#include "content/public/common/platform_notification_data.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/permissions/permission_status.mojom.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

using ::testing::_;
using ::testing::Return;
using content::NotificationResources;
using content::PlatformNotificationData;
using message_center::Notification;

namespace {

const char kNotificationId[] = "my-notification-id";
const int kNotificationVibrationPattern[] = { 100, 200, 300 };

class TestingProfileWithPermissionManager : public TestingProfile {
 public:
  TestingProfileWithPermissionManager()
      : permission_manager_(
            std::make_unique<
                testing::NiceMock<content::MockPermissionManager>>()) {}

  ~TestingProfileWithPermissionManager() override = default;

  // Sets the notification permission status to |permission_status|.
  void SetNotificationPermissionStatus(
      blink::mojom::PermissionStatus permission_status) {
    ON_CALL(*permission_manager_,
            GetPermissionStatus(content::PermissionType::NOTIFICATIONS, _, _))
        .WillByDefault(Return(permission_status));
  }

  // TestingProfile overrides:
  content::PermissionControllerDelegate* GetPermissionControllerDelegate()
      override {
    return permission_manager_.get();
  }

 private:
  std::unique_ptr<content::MockPermissionManager> permission_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestingProfileWithPermissionManager);
};

}  // namespace

class PlatformNotificationServiceTest : public testing::Test {
 public:
  void SetUp() override {
    display_service_tester_ =
        std::make_unique<NotificationDisplayServiceTester>(&profile_);

    mock_logger_ = static_cast<MockNotificationMetricsLogger*>(
        NotificationMetricsLoggerFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                &profile_, &MockNotificationMetricsLogger::FactoryForTests));
  }

  void TearDown() override {
    display_service_tester_.reset();
  }

 protected:
  // Returns the Platform Notification Service these unit tests are for.
  PlatformNotificationServiceImpl* service() const {
    return PlatformNotificationServiceImpl::GetInstance();
  }

  size_t GetNotificationCountForType(NotificationHandler::Type type) {
    return display_service_tester_->GetDisplayedNotificationsForType(type)
        .size();
  }

  Notification GetDisplayedNotificationForType(NotificationHandler::Type type) {
    std::vector<Notification> notifications =
        display_service_tester_->GetDisplayedNotificationsForType(type);
    DCHECK_EQ(1u, notifications.size());

    return notifications[0];
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;

  TestingProfileWithPermissionManager profile_;

  std::unique_ptr<NotificationDisplayServiceTester> display_service_tester_;

  // Owned by the |profile_| as a keyed service.
  MockNotificationMetricsLogger* mock_logger_;
};

TEST_F(PlatformNotificationServiceTest, DisplayNonPersistentThenClose) {
  PlatformNotificationData notification_data;
  notification_data.title = base::ASCIIToUTF16("My Notification");
  notification_data.body = base::ASCIIToUTF16("Hello, world!");

  service()->DisplayNotification(&profile_, kNotificationId,
                                 GURL("https://chrome.com/"), notification_data,
                                 NotificationResources());

  EXPECT_EQ(1u, GetNotificationCountForType(
                    NotificationHandler::Type::WEB_NON_PERSISTENT));

  service()->CloseNotification(&profile_, kNotificationId);

  EXPECT_EQ(0u, GetNotificationCountForType(
                    NotificationHandler::Type::WEB_NON_PERSISTENT));
}

TEST_F(PlatformNotificationServiceTest, DisplayPersistentThenClose) {
  PlatformNotificationData notification_data;
  notification_data.title = base::ASCIIToUTF16("My notification's title");
  notification_data.body = base::ASCIIToUTF16("Hello, world!");

  EXPECT_CALL(*mock_logger_, LogPersistentNotificationShown());
  service()->DisplayPersistentNotification(
      &profile_, kNotificationId, GURL() /* service_worker_scope */,
      GURL("https://chrome.com/"), notification_data, NotificationResources());

  ASSERT_EQ(1u, GetNotificationCountForType(
                    NotificationHandler::Type::WEB_PERSISTENT));

  Notification notification = GetDisplayedNotificationForType(
      NotificationHandler::Type::WEB_PERSISTENT);
  EXPECT_EQ("https://chrome.com/", notification.origin_url().spec());
  EXPECT_EQ("My notification's title",
      base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ("Hello, world!",
      base::UTF16ToUTF8(notification.message()));

  service()->ClosePersistentNotification(&profile_, kNotificationId);
  EXPECT_EQ(0u, GetNotificationCountForType(
                    NotificationHandler::Type::WEB_PERSISTENT));
}

TEST_F(PlatformNotificationServiceTest, OnPersistentNotificationClick) {
  EXPECT_CALL(*mock_logger_, LogPersistentNotificationClickWithoutPermission());
  profile_.SetNotificationPermissionStatus(
      blink::mojom::PermissionStatus::DENIED);

  service()->OnPersistentNotificationClick(
      &profile_, "jskdcjdslkcjlds", GURL("https://example.com/"), base::nullopt,
      base::nullopt, base::DoNothing());
}

TEST_F(PlatformNotificationServiceTest, OnPersistentNotificationClosedByUser) {
  EXPECT_CALL(*mock_logger_, LogPersistentNotificationClosedByUser());
  service()->OnPersistentNotificationClose(
      &profile_, "some_random_id_123", GURL("https://example.com/"),
      true /* by_user */, base::DoNothing());
}

TEST_F(PlatformNotificationServiceTest,
       OnPersistentNotificationClosedProgrammatically) {
  EXPECT_CALL(*mock_logger_, LogPersistentNotificationClosedProgrammatically());
  service()->OnPersistentNotificationClose(
      &profile_, "some_random_id_738", GURL("https://example.com/"),
      false /* by_user */, base::DoNothing());
}

TEST_F(PlatformNotificationServiceTest, DisplayNonPersistentPropertiesMatch) {
  std::vector<int> vibration_pattern(
      kNotificationVibrationPattern,
      kNotificationVibrationPattern + arraysize(kNotificationVibrationPattern));

  PlatformNotificationData notification_data;
  notification_data.title = base::ASCIIToUTF16("My notification's title");
  notification_data.body = base::ASCIIToUTF16("Hello, world!");
  notification_data.vibration_pattern = vibration_pattern;
  notification_data.silent = true;

  service()->DisplayNotification(&profile_, kNotificationId,
                                 GURL("https://chrome.com/"), notification_data,
                                 NotificationResources());

  ASSERT_EQ(1u, GetNotificationCountForType(
                    NotificationHandler::Type::WEB_NON_PERSISTENT));

  Notification notification = GetDisplayedNotificationForType(
      NotificationHandler::Type::WEB_NON_PERSISTENT);
  EXPECT_EQ("https://chrome.com/", notification.origin_url().spec());
  EXPECT_EQ("My notification's title",
      base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ("Hello, world!",
      base::UTF16ToUTF8(notification.message()));

  EXPECT_THAT(notification.vibration_pattern(),
      testing::ElementsAreArray(kNotificationVibrationPattern));

  EXPECT_TRUE(notification.silent());
}

TEST_F(PlatformNotificationServiceTest, DisplayPersistentPropertiesMatch) {
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

  EXPECT_CALL(*mock_logger_, LogPersistentNotificationShown());
  service()->DisplayPersistentNotification(
      &profile_, kNotificationId, GURL() /* service_worker_scope */,
      GURL("https://chrome.com/"), notification_data, notification_resources);

  ASSERT_EQ(1u, GetNotificationCountForType(
                    NotificationHandler::Type::WEB_PERSISTENT));

  Notification notification = GetDisplayedNotificationForType(
      NotificationHandler::Type::WEB_PERSISTENT);
  EXPECT_EQ("https://chrome.com/", notification.origin_url().spec());
  EXPECT_EQ("My notification's title", base::UTF16ToUTF8(notification.title()));
  EXPECT_EQ("Hello, world!", base::UTF16ToUTF8(notification.message()));

  EXPECT_THAT(notification.vibration_pattern(),
              testing::ElementsAreArray(kNotificationVibrationPattern));

  EXPECT_TRUE(notification.silent());

  const auto& buttons = notification.buttons();
  ASSERT_EQ(2u, buttons.size());
  EXPECT_EQ("Button 1", base::UTF16ToUTF8(buttons[0].title));
  EXPECT_FALSE(buttons[0].placeholder);
  EXPECT_EQ("Button 2", base::UTF16ToUTF8(buttons[1].title));
  EXPECT_TRUE(buttons[1].placeholder);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)

TEST_F(PlatformNotificationServiceTest, DisplayNameForContextMessage) {
  base::string16 display_name = service()->DisplayNameForContextMessage(
      &profile_, GURL("https://chrome.com/"));

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
      extensions::ExtensionRegistry::Get(&profile_);
  EXPECT_TRUE(registry->AddEnabled(extension));

  display_name = service()->DisplayNameForContextMessage(
      &profile_,
      GURL("chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html"));
  EXPECT_EQ("NotificationTest", base::UTF16ToUTF8(display_name));
}

TEST_F(PlatformNotificationServiceTest, CreateNotificationFromData) {
  PlatformNotificationData notification_data;
  notification_data.title = base::ASCIIToUTF16("My Notification");
  notification_data.body = base::ASCIIToUTF16("Hello, world!");
  GURL origin("https://chrome.com/");

  Notification notification = service()->CreateNotificationFromData(
      &profile_, origin, "id", notification_data, NotificationResources(),
      nullptr /* delegate */);
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
      extensions::ExtensionRegistry::Get(&profile_);
  EXPECT_TRUE(registry->AddEnabled(extension));

  notification = service()->CreateNotificationFromData(
      &profile_,
      GURL("chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html"),
      "id", notification_data, NotificationResources(), nullptr /* delegate */);
  EXPECT_EQ("NotificationTest",
            base::UTF16ToUTF8(notification.context_message()));
}

// Expect each call to ReadNextPersistentNotificationId to return a larger
// value.
TEST_F(PlatformNotificationServiceTest, NextPersistentNotificationId) {
  int64_t first_id = service()->ReadNextPersistentNotificationId(&profile_);
  int64_t second_id = service()->ReadNextPersistentNotificationId(&profile_);
  EXPECT_LT(first_id, second_id);
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
