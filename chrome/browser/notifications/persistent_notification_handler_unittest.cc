// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/persistent_notification_handler.h"

#include <memory>

#include "base/macros.h"
#include "chrome/browser/notifications/metrics/mock_notification_metrics_logger.h"
#include "chrome/browser/notifications/metrics/notification_metrics_logger_factory.h"
#include "chrome/browser/notifications/notification_permission_context.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/permission_type.h"
#include "content/public/test/mock_permission_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/permissions/permission_status.mojom.h"

using ::testing::_;
using ::testing::Return;

namespace {

const char kExampleNotificationId[] = "example_notification_id";
const char kExampleOrigin[] = "https://example.com";

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

class PersistentNotificationHandlerTest : public ::testing::Test {
 public:
  PersistentNotificationHandlerTest() : origin_(kExampleOrigin) {}

  ~PersistentNotificationHandlerTest() override = default;

  // ::testing::Test overrides:
  void SetUp() override {
    mock_logger_ = static_cast<MockNotificationMetricsLogger*>(
        NotificationMetricsLoggerFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                &profile_, &MockNotificationMetricsLogger::FactoryForTests));
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfileWithPermissionManager profile_;

  // The origin for which these tests are being run.
  GURL origin_;

  // Owned by the |profile_| as a keyed service.
  MockNotificationMetricsLogger* mock_logger_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(PersistentNotificationHandlerTest);
};

TEST_F(PersistentNotificationHandlerTest, OnClick_WithoutPermission) {
  EXPECT_CALL(*mock_logger_, LogPersistentNotificationClickWithoutPermission());
  profile_.SetNotificationPermissionStatus(
      blink::mojom::PermissionStatus::DENIED);

  std::unique_ptr<NotificationHandler> handler =
      std::make_unique<PersistentNotificationHandler>();

  handler->OnClick(&profile_, origin_, kExampleNotificationId,
                   base::nullopt /* action_index */, base::nullopt /* reply */,
                   base::DoNothing());
}

TEST_F(PersistentNotificationHandlerTest, OnClose_ByUser) {
  EXPECT_CALL(*mock_logger_, LogPersistentNotificationClosedByUser());

  std::unique_ptr<NotificationHandler> handler =
      std::make_unique<PersistentNotificationHandler>();

  handler->OnClose(&profile_, origin_, kExampleNotificationId,
                   /* by_user= */ true, base::DoNothing());
}

TEST_F(PersistentNotificationHandlerTest, OnClose_Programmatically) {
  EXPECT_CALL(*mock_logger_, LogPersistentNotificationClosedProgrammatically());

  std::unique_ptr<NotificationHandler> handler =
      std::make_unique<PersistentNotificationHandler>();

  handler->OnClose(&profile_, origin_, kExampleNotificationId,
                   /* by_user= */ false, base::DoNothing());
}

TEST_F(PersistentNotificationHandlerTest, DisableNotifications) {
  std::unique_ptr<NotificationPermissionContext> permission_context =
      std::make_unique<NotificationPermissionContext>(&profile_);

  ASSERT_EQ(permission_context
                ->GetPermissionStatus(nullptr /* render_frame_host */, origin_,
                                      origin_)
                .content_setting,
            CONTENT_SETTING_ASK);

  std::unique_ptr<NotificationHandler> handler =
      std::make_unique<PersistentNotificationHandler>();
  handler->DisableNotifications(&profile_, origin_);

  ASSERT_EQ(permission_context
                ->GetPermissionStatus(nullptr /* render_frame_host */, origin_,
                                      origin_)
                .content_setting,
            CONTENT_SETTING_BLOCK);
}

}  // namespace
