// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_linux.h"

#include <memory>

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/notifications/notification.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kFreedesktopNotificationsName[] = "org.freedesktop.Notifications";
const char kFreedesktopNotificationsPath[] = "/org/freedesktop/Notifications";

ACTION_P(RegisterSignalCallback, callback_addr) {
  *callback_addr = arg2;
  arg3.Run("" /* interface_name */, "" /* signal_name */, true /* success */);
}

}  // namespace

class NotificationPlatformBridgeLinuxTest : public testing::Test {
 public:
  NotificationPlatformBridgeLinuxTest() = default;
  ~NotificationPlatformBridgeLinuxTest() override = default;

  void SetUp() override {
    mock_bus_ = new dbus::MockBus(dbus::Bus::Options());
    mock_notification_proxy_ = new testing::StrictMock<dbus::MockObjectProxy>(
        mock_bus_.get(), kFreedesktopNotificationsName,
        dbus::ObjectPath(kFreedesktopNotificationsPath));

    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy(kFreedesktopNotificationsName,
                               dbus::ObjectPath(kFreedesktopNotificationsPath)))
        .WillOnce(testing::Return(mock_notification_proxy_.get()));

    EXPECT_CALL(*mock_notification_proxy_.get(),
                ConnectToSignal(kFreedesktopNotificationsName, "ActionInvoked",
                                testing::_, testing::_))
        .WillOnce(RegisterSignalCallback(&action_invoked_callback_));

    EXPECT_CALL(*mock_notification_proxy_.get(),
                ConnectToSignal(kFreedesktopNotificationsName,
                                "NotificationClosed", testing::_, testing::_))
        .WillOnce(RegisterSignalCallback(&notification_closed_callback_));

    notification_bridge_linux_ =
        base::WrapUnique(new NotificationPlatformBridgeLinux(mock_bus_));
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
  content::TestBrowserThreadBundle thread_bundle_;

  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_notification_proxy_;

  base::Callback<void(dbus::Signal*)> action_invoked_callback_;
  base::Callback<void(dbus::Signal*)> notification_closed_callback_;

  std::unique_ptr<NotificationPlatformBridgeLinux> notification_bridge_linux_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationPlatformBridgeLinuxTest);
};

TEST_F(NotificationPlatformBridgeLinuxTest, SetUpAndTearDown) {}
