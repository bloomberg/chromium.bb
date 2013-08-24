// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/notifications/message_center_tray_bridge.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#import "ui/base/test/ui_cocoa_test_helper.h"
#import "ui/message_center/cocoa/status_item_view.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"

class MessageCenterTrayBridgeTest : public ui::CocoaTest {
 public:
  virtual void SetUp() OVERRIDE {
    ui::CocoaTest::SetUp();

    message_center::MessageCenter::Initialize();
    center_ = message_center::MessageCenter::Get();

    bridge_.reset(new MessageCenterTrayBridge(center_));
  }

  virtual void TearDown() OVERRIDE {
    bridge_.reset();
    message_center::MessageCenter::Shutdown();
    ui::CocoaTest::TearDown();
  }

  MCStatusItemView* status_item() { return bridge_->status_item_view_.get(); }

 protected:
  base::MessageLoop message_loop_;
  message_center::MessageCenter* center_;  // Weak, global.
  scoped_ptr<MessageCenterTrayBridge> bridge_;
};

TEST_F(MessageCenterTrayBridgeTest, StatusItemOnlyAfterFirstNotification) {
  EXPECT_FALSE(status_item());

  message_center::RichNotificationData data;
  data.priority = -1;
  scoped_ptr<message_center::Notification> notification(
      new message_center::Notification(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          "1",
          ASCIIToUTF16("First notification"),
          ASCIIToUTF16("This is a simple test."),
          gfx::Image(),
          string16(),
          message_center::NotifierId(),
          data,
          NULL));
  center_->AddNotification(notification.Pass());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(status_item());
  EXPECT_EQ(1u, [status_item() unreadCount]);

  center_->RemoveNotification("1", /*by_user=*/true);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(status_item());
}
