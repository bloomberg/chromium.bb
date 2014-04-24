// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/notifications/message_center_tray_bridge.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"
#import "ui/message_center/cocoa/status_item_view.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notifier_settings.h"

class MessageCenterTrayBridgeTest : public ui::CocoaTest {
 public:
  virtual void SetUp() OVERRIDE {
    ui::CocoaTest::SetUp();

    local_state_.reset(
        new ScopedTestingLocalState(TestingBrowserProcess::GetGlobal()));
    message_center::MessageCenter::Initialize();
    center_ = message_center::MessageCenter::Get();

    bridge_.reset(new MessageCenterTrayBridge(center_));
  }

  virtual void TearDown() OVERRIDE {
    bridge_.reset();
    message_center::MessageCenter::Shutdown();
    local_state_.reset();
    initializer_.reset();
    ui::CocoaTest::TearDown();
  }

  MCStatusItemView* status_item() { return bridge_->status_item_view_.get(); }

 protected:
  scoped_ptr<message_center::Notification> GetNotification() {
    message_center::RichNotificationData data;
    data.priority = -1;
    return make_scoped_ptr(new message_center::Notification(
        message_center::NOTIFICATION_TYPE_SIMPLE,
        "1",
        base::ASCIIToUTF16("First notification"),
        base::ASCIIToUTF16("This is a simple test."),
        gfx::Image(),
        base::string16(),
        message_center::NotifierId(),
        data,
        NULL));
  }

  TestingPrefServiceSimple* local_state() { return local_state_->Get(); }

  scoped_ptr<TestingBrowserProcessInitializer> initializer_;
  scoped_ptr<ScopedTestingLocalState> local_state_;

  base::MessageLoop message_loop_;
  message_center::MessageCenter* center_;  // Weak, global.
  scoped_ptr<MessageCenterTrayBridge> bridge_;
};

class MessageCenterTrayBridgeTestPrefNever
    : public MessageCenterTrayBridgeTest {
 public:
  virtual void SetUp() OVERRIDE {
    MessageCenterTrayBridgeTest::SetUp();
    local_state()->SetBoolean(prefs::kMessageCenterShowIcon, false);
  }
};

TEST_F(MessageCenterTrayBridgeTest, StatusItemOnlyAfterFirstNotification) {
  EXPECT_FALSE(status_item());

  center_->AddNotification(GetNotification());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(status_item());
  EXPECT_EQ(1u, [status_item() unreadCount]);

  center_->RemoveNotification("1", /*by_user=*/true);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(status_item());
}

TEST_F(MessageCenterTrayBridgeTest, StatusItemAppearsWithPrefChange) {
  EXPECT_FALSE(status_item());
  local_state()->SetBoolean(prefs::kMessageCenterShowIcon, false);
  EXPECT_FALSE(status_item());
  local_state()->SetBoolean(prefs::kMessageCenterShowIcon, true);
  EXPECT_TRUE(status_item());
}

TEST_F(MessageCenterTrayBridgeTest, StatusItemDisappearsWithPrefChange) {
  EXPECT_FALSE(status_item());
  center_->AddNotification(GetNotification());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(status_item());
  local_state()->SetBoolean(prefs::kMessageCenterShowIcon, false);
  EXPECT_FALSE(status_item());
  local_state()->SetBoolean(prefs::kMessageCenterShowIcon, true);
  EXPECT_TRUE(status_item());
}

TEST_F(MessageCenterTrayBridgeTestPrefNever, StatusItemNever) {
  EXPECT_FALSE(status_item());

  center_->AddNotification(GetNotification());

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(status_item());

  center_->RemoveNotification("1", /*by_user=*/true);

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(status_item());
}

TEST_F(MessageCenterTrayBridgeTestPrefNever, StatusItemBackWithPref) {
  EXPECT_FALSE(status_item());

  center_->AddNotification(GetNotification());

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(status_item());

  center_->RemoveNotification("1", /*by_user=*/true);

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(status_item());

  local_state()->SetBoolean(prefs::kMessageCenterShowIcon, true);

  EXPECT_TRUE(status_item());
}
