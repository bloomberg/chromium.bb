// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_message_handler_desktop.h"

#include <memory>

#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/stub_notification_display_service.h"
#include "chrome/browser/sharing/proto/shared_clipboard_message.pb.h"
#include "chrome/browser/sharing/sharing_fcm_handler.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"

namespace {

const char kText[] = "clipboard";
const char kClientName[] = "Foo's Pixel";

class MockSharingService : public SharingService {
 public:
  explicit MockSharingService(
      NotificationDisplayService* notification_display_service)
      : SharingService(
            /* sync_prefs= */ nullptr,
            /* vapid_key_manager= */ nullptr,
            /* sharing_device_registration= */ nullptr,
            /* fcm_sender= */ nullptr,
            std::make_unique<SharingFCMHandler>(nullptr, nullptr, nullptr),
            /* gcm_driver= */ nullptr,
            /* device_info_tracker= */ nullptr,
            /* local_device_info_provider= */ nullptr,
            /* sync_service */ nullptr,
            notification_display_service) {}

  ~MockSharingService() override = default;

  MOCK_CONST_METHOD1(
      GetDeviceByGuid,
      std::unique_ptr<syncer::DeviceInfo>(const std::string& guid));
};

class SharedClipboardMessageHandlerTest : public testing::Test {
 public:
  SharedClipboardMessageHandlerTest() = default;

  void SetUp() override {
    notification_display_service_ =
        std::make_unique<StubNotificationDisplayService>(&profile_);
    sharing_service_ = std::make_unique<MockSharingService>(
        notification_display_service_.get());
    message_handler_ = std::make_unique<SharedClipboardMessageHandlerDesktop>(
        sharing_service_.get(), notification_display_service_.get());
  }

  void TearDown() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  chrome_browser_sharing::SharingMessage CreateMessage(const std::string guid) {
    chrome_browser_sharing::SharingMessage message;
    message.set_sender_guid(guid);
    message.mutable_shared_clipboard_message()->set_text(kText);
    return message;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<StubNotificationDisplayService> notification_display_service_;
  std::unique_ptr<MockSharingService> sharing_service_;
  std::unique_ptr<SharedClipboardMessageHandlerDesktop> message_handler_;
  TestingProfile profile_;

  DISALLOW_COPY_AND_ASSIGN(SharedClipboardMessageHandlerTest);
};

}  // namespace

TEST_F(SharedClipboardMessageHandlerTest, MessageCopiedToClipboard) {
  std::string guid = base::GenerateGUID();
  {
    EXPECT_CALL(*sharing_service_, GetDeviceByGuid(guid))
        .WillOnce(
            [](const std::string& guid) -> std::unique_ptr<syncer::DeviceInfo> {
              return nullptr;
            });
    message_handler_->OnMessage(CreateMessage(guid));
  }
  EXPECT_TRUE(notification_display_service_
                  ->GetDisplayedNotificationsForType(
                      NotificationHandler::Type::TRANSIENT)
                  .empty());

  base::string16 text;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &text);
  EXPECT_EQ(base::UTF16ToUTF8(text), kText);
}

TEST_F(SharedClipboardMessageHandlerTest, NotificationDisplayed) {
  std::string guid = base::GenerateGUID();
  {
    EXPECT_CALL(*sharing_service_, GetDeviceByGuid(guid))
        .WillOnce(
            [](const std::string& guid) -> std::unique_ptr<syncer::DeviceInfo> {
              return std::make_unique<syncer::DeviceInfo>(
                  base::GenerateGUID(), kClientName,
                  /* chrome_version= */ "78.0.0.0",
                  /* sync_user_agent= */ "Chrome",
                  sync_pb::SyncEnums::TYPE_LINUX,
                  /* signin_scoped_device_id= */ base::GenerateGUID(),
                  base::Time::Now(),
                  /* send_tab_to_self_receiving_enabled= */ true);
            });
    message_handler_->OnMessage(CreateMessage(guid));
  }
  auto notifications =
      notification_display_service_->GetDisplayedNotificationsForType(
          NotificationHandler::Type::SHARING);
  ASSERT_EQ(notifications.size(), 1u);

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());
  EXPECT_EQ("Text shared from " + std::string(kClientName),
            base::UTF16ToUTF8(notification.title()));
}
