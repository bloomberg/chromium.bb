// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_linux.h"

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/i18n/number_formatting.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;
using testing::StrictMock;

namespace {

const char kFreedesktopNotificationsName[] = "org.freedesktop.Notifications";
const char kFreedesktopNotificationsPath[] = "/org/freedesktop/Notifications";

class NotificationBuilder {
 public:
  explicit NotificationBuilder(const std::string& id)
      : notification_(message_center::NOTIFICATION_TYPE_SIMPLE,
                      base::string16(),
                      base::string16(),
                      gfx::Image(),
                      message_center::NotifierId(GURL()),
                      base::string16(),
                      GURL(),
                      id,
                      message_center::RichNotificationData(),
                      new MockNotificationDelegate(id)) {}

  Notification GetResult() { return notification_; }

  NotificationBuilder& SetItems(
      const std::vector<message_center::NotificationItem>& items) {
    notification_.set_items(items);
    return *this;
  }

  NotificationBuilder& SetNeverTimeout(bool never_timeout) {
    notification_.set_never_timeout(never_timeout);
    return *this;
  }

  NotificationBuilder& SetProgress(int progress) {
    notification_.set_progress(progress);
    return *this;
  }

  NotificationBuilder& SetTitle(const base::string16& title) {
    notification_.set_title(title);
    return *this;
  }

  NotificationBuilder& SetType(message_center::NotificationType type) {
    notification_.set_type(type);
    return *this;
  }

 private:
  Notification notification_;
};

struct NotificationRequest {
  std::string summary;
  std::string body;
  int32_t expire_timeout = 0;
};

NotificationRequest ParseRequest(dbus::MethodCall* method_call) {
  // The "Notify" message must have type (susssasa{sv}i).
  // https://developer.gnome.org/notification-spec/#command-notify
  NotificationRequest request;

  dbus::MessageReader reader(method_call);
  std::string str;
  uint32_t uint32;
  EXPECT_TRUE(reader.PopString(&str));              // app_name
  EXPECT_TRUE(reader.PopUint32(&uint32));           // replaces_id
  EXPECT_TRUE(reader.PopString(&str));              // app_icon
  EXPECT_TRUE(reader.PopString(&request.summary));  // summary
  EXPECT_TRUE(reader.PopString(&request.body));     // body

  {
    dbus::MessageReader actions_reader(nullptr);
    EXPECT_TRUE(reader.PopArray(&actions_reader));
    while (actions_reader.HasMoreData()) {
      // Actions come in pairs.
      EXPECT_TRUE(actions_reader.PopString(&str));
      EXPECT_TRUE(actions_reader.PopString(&str));
    }
  }

  {
    dbus::MessageReader hints_reader(nullptr);
    EXPECT_TRUE(reader.PopArray(&hints_reader));
    while (hints_reader.HasMoreData()) {
      dbus::MessageReader dict_entry_reader(nullptr);
      EXPECT_TRUE(hints_reader.PopDictEntry(&dict_entry_reader));
      EXPECT_TRUE(dict_entry_reader.PopString(&str));
      dbus::MessageReader variant_reader(nullptr);
      EXPECT_TRUE(dict_entry_reader.PopVariant(&variant_reader));
      EXPECT_FALSE(dict_entry_reader.HasMoreData());
    }
  }

  EXPECT_TRUE(reader.PopInt32(&request.expire_timeout));
  EXPECT_FALSE(reader.HasMoreData());

  return request;
}

dbus::Response* GetIdResponse(uint32_t id) {
  dbus::Response* response = dbus::Response::CreateEmpty().release();
  dbus::MessageWriter writer(response);
  writer.AppendUint32(id);
  return response;
}

ACTION_P(RegisterSignalCallback, callback_addr) {
  *callback_addr = arg2;
  arg3.Run("" /* interface_name */, "" /* signal_name */, true /* success */);
}

ACTION_P(OnGetCapabilities, capabilities) {
  // MockObjectProxy::CallMethodAndBlock will wrap the return value in
  // a unique_ptr.
  dbus::Response* response = dbus::Response::CreateEmpty().release();
  dbus::MessageWriter writer(response);
  writer.AppendArrayOfStrings(capabilities);
  return response;
}

ACTION_P(OnGetServerInformation, spec_version) {
  dbus::Response* response = dbus::Response::CreateEmpty().release();
  dbus::MessageWriter writer(response);
  writer.AppendString("");  // name
  writer.AppendString("");  // vendor
  writer.AppendString("");  // version
  writer.AppendString(spec_version);
  return response;
}

ACTION_P2(OnNotify, verifier, id) {
  verifier(ParseRequest(arg0));
  return GetIdResponse(id);
}

ACTION(OnCloseNotification) {
  // The "CloseNotification" message must have type (u).
  // https://developer.gnome.org/notification-spec/#command-close-notification
  dbus::MethodCall* method_call = arg0;
  dbus::MessageReader reader(method_call);
  uint32_t uint32;
  EXPECT_TRUE(reader.PopUint32(&uint32));
  EXPECT_FALSE(reader.HasMoreData());

  return dbus::Response::CreateEmpty().release();
}

// Matches a method call to the specified dbus target.
MATCHER_P(Calls, member, "") {
  return arg->GetMember() == member;
}

}  // namespace

class NotificationPlatformBridgeLinuxTest : public testing::Test {
 public:
  NotificationPlatformBridgeLinuxTest() = default;
  ~NotificationPlatformBridgeLinuxTest() override = default;

  void SetUp() override {
    mock_bus_ = new dbus::MockBus(dbus::Bus::Options());
    mock_notification_proxy_ = new StrictMock<dbus::MockObjectProxy>(
        mock_bus_.get(), kFreedesktopNotificationsName,
        dbus::ObjectPath(kFreedesktopNotificationsPath));

    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy(kFreedesktopNotificationsName,
                               dbus::ObjectPath(kFreedesktopNotificationsPath)))
        .WillOnce(Return(mock_notification_proxy_.get()));

    EXPECT_CALL(*mock_notification_proxy_.get(),
                MockCallMethodAndBlock(Calls("GetCapabilities"), _))
        .WillOnce(OnGetCapabilities(std::vector<std::string>{"body"}));

    EXPECT_CALL(*mock_notification_proxy_.get(),
                MockCallMethodAndBlock(Calls("GetServerInformation"), _))
        .WillOnce(OnGetServerInformation("1.2"));

    EXPECT_CALL(
        *mock_notification_proxy_.get(),
        ConnectToSignal(kFreedesktopNotificationsName, "ActionInvoked", _, _))
        .WillOnce(RegisterSignalCallback(&action_invoked_callback_));

    EXPECT_CALL(*mock_notification_proxy_.get(),
                ConnectToSignal(kFreedesktopNotificationsName,
                                "NotificationClosed", _, _))
        .WillOnce(RegisterSignalCallback(&notification_closed_callback_));
  }

  void TearDown() override {
    EXPECT_CALL(*mock_bus_.get(), ShutdownAndBlock());
    notification_bridge_linux_->CleanUp();
    content::RunAllBlockingPoolTasksUntilIdle();
    notification_bridge_linux_.reset();
    mock_notification_proxy_ = nullptr;
    mock_bus_ = nullptr;
  }

 protected:
  void CreateNotificationBridgeLinux() {
    notification_bridge_linux_ =
        base::WrapUnique(new NotificationPlatformBridgeLinux(mock_bus_));
  }

  content::TestBrowserThreadBundle thread_bundle_;

  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_notification_proxy_;

  base::Callback<void(dbus::Signal*)> action_invoked_callback_;
  base::Callback<void(dbus::Signal*)> notification_closed_callback_;

  std::unique_ptr<NotificationPlatformBridgeLinux> notification_bridge_linux_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationPlatformBridgeLinuxTest);
};

TEST_F(NotificationPlatformBridgeLinuxTest, SetUpAndTearDown) {
  CreateNotificationBridgeLinux();
}

TEST_F(NotificationPlatformBridgeLinuxTest, NotifyAndCloseFormat) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              MockCallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify([](const NotificationRequest&) {}, 1));
  EXPECT_CALL(*mock_notification_proxy_.get(),
              MockCallMethodAndBlock(Calls("CloseNotification"), _))
      .WillOnce(OnCloseNotification());

  CreateNotificationBridgeLinux();
  notification_bridge_linux_->Display(NotificationCommon::PERSISTENT, "", "",
                                      false,
                                      NotificationBuilder("").GetResult());
  notification_bridge_linux_->Close("", "");
}

TEST_F(NotificationPlatformBridgeLinuxTest, ProgressPercentageAddedToSummary) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              MockCallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [](const NotificationRequest& request) {
            EXPECT_EQ(
                base::UTF16ToUTF8(base::FormatPercent(42)) + " - The Title",
                request.summary);
          },
          1));

  CreateNotificationBridgeLinux();
  notification_bridge_linux_->Display(
      NotificationCommon::PERSISTENT, "", "", false,
      NotificationBuilder("")
          .SetType(message_center::NOTIFICATION_TYPE_PROGRESS)
          .SetProgress(42)
          .SetTitle(base::UTF8ToUTF16("The Title"))
          .GetResult());
}

TEST_F(NotificationPlatformBridgeLinuxTest, NotificationListItemsInBody) {
  EXPECT_CALL(*mock_notification_proxy_.get(),
              MockCallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [](const NotificationRequest& request) {
            EXPECT_EQ("abc - 123\ndef - 456", request.body);
          },
          1));

  CreateNotificationBridgeLinux();
  notification_bridge_linux_->Display(
      NotificationCommon::PERSISTENT, "", "", false,
      NotificationBuilder("")
          .SetType(message_center::NOTIFICATION_TYPE_MULTIPLE)
          .SetItems(std::vector<message_center::NotificationItem>{
              {base::UTF8ToUTF16("abc"), base::UTF8ToUTF16("123")},
              {base::UTF8ToUTF16("def"), base::UTF8ToUTF16("456")}})
          .GetResult());
}

TEST_F(NotificationPlatformBridgeLinuxTest, NotificationTimeouts) {
  const int32_t kExpireTimeoutDefault = -1;
  const int32_t kExpireTimeoutNever = 0;
  EXPECT_CALL(*mock_notification_proxy_.get(),
              MockCallMethodAndBlock(Calls("Notify"), _))
      .WillOnce(OnNotify(
          [=](const NotificationRequest& request) {
            EXPECT_EQ(kExpireTimeoutDefault, request.expire_timeout);
          },
          1))
      .WillOnce(OnNotify(
          [=](const NotificationRequest& request) {
            EXPECT_EQ(kExpireTimeoutNever, request.expire_timeout);
          },
          2));

  CreateNotificationBridgeLinux();
  notification_bridge_linux_->Display(
      NotificationCommon::PERSISTENT, "", "", false,
      NotificationBuilder("1").SetNeverTimeout(false).GetResult());
  notification_bridge_linux_->Display(
      NotificationCommon::PERSISTENT, "", "", false,
      NotificationBuilder("2").SetNeverTimeout(true).GetResult());
}
