// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/new_unified_message_center_view.h"

#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/controls/scroll_view.h"

using message_center::MessageCenter;
using message_center::MessageView;
using message_center::Notification;

namespace ash {

namespace {

class DummyEvent : public ui::Event {
 public:
  DummyEvent() : Event(ui::ET_UNKNOWN, base::TimeTicks(), 0) {}
  ~DummyEvent() override = default;
};

}  // namespace

class NewUnifiedMessageCenterViewTest : public AshTestBase,
                                        public views::ViewObserver {
 public:
  NewUnifiedMessageCenterViewTest() = default;
  ~NewUnifiedMessageCenterViewTest() override = default;

  // AshTestBase:
  void TearDown() override {
    message_center_view_.reset();
    AshTestBase::TearDown();
  }

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* view) override {
    view->SetBoundsRect(gfx::Rect(view->GetPreferredSize()));
    view->Layout();
    ++size_changed_count_;
  }

 protected:
  std::string AddNotification() {
    std::string id = base::IntToString(id_++);
    MessageCenter::Get()->AddNotification(std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_BASE_FORMAT, id,
        base::UTF8ToUTF16("test title"), base::UTF8ToUTF16("test message"),
        gfx::Image(), base::string16() /* display_source */, GURL(),
        message_center::NotifierId(), message_center::RichNotificationData(),
        new message_center::NotificationDelegate()));
    return id;
  }

  void CreateMessageCenterView() {
    message_center_view_ = std::make_unique<NewUnifiedMessageCenterView>();
    message_center_view_->AddObserver(this);
    message_center_view_->SetMaxHeight(500);
    OnViewPreferredSizeChanged(message_center_view_.get());
    size_changed_count_ = 0;
  }

  UnifiedMessageListView* GetMessageListView() {
    return message_center_view()->message_list_view_;
  }

  views::ScrollView* GetScroller() { return message_center_view()->scroller_; }

  views::View* GetScrollerContents() {
    return message_center_view()->scroller_->contents();
  }

  NewUnifiedMessageCenterView* message_center_view() {
    return message_center_view_.get();
  }

  int size_changed_count() const { return size_changed_count_; }

 private:
  int id_ = 0;
  int size_changed_count_ = 0;

  std::unique_ptr<NewUnifiedMessageCenterView> message_center_view_;

  DISALLOW_COPY_AND_ASSIGN(NewUnifiedMessageCenterViewTest);
};

TEST_F(NewUnifiedMessageCenterViewTest, AddAndRemoveNotification) {
  CreateMessageCenterView();
  EXPECT_FALSE(message_center_view()->visible());

  auto id0 = AddNotification();
  EXPECT_TRUE(message_center_view()->visible());

  MessageCenter::Get()->RemoveNotification(id0, true /* by_user */);
  EXPECT_FALSE(message_center_view()->visible());
}

TEST_F(NewUnifiedMessageCenterViewTest, NotVisibleWhenLocked) {
  AddNotification();
  AddNotification();

  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  CreateMessageCenterView();

  EXPECT_FALSE(message_center_view()->visible());
}

TEST_F(NewUnifiedMessageCenterViewTest, ClearAllPressed) {
  AddNotification();
  AddNotification();
  CreateMessageCenterView();
  EXPECT_TRUE(message_center_view()->visible());

  // ScrollView fills MessageCenterView.
  EXPECT_EQ(message_center_view()->bounds(), GetScroller()->bounds());
  EXPECT_EQ(GetMessageListView()->GetPreferredSize().width(),
            message_center_view()->GetPreferredSize().width());

  // MessageCenterView returns smaller height to hide Clear All button.
  EXPECT_EQ(kUnifiedNotificationCenterSpacing,
            message_center_view()->GetPreferredSize().height() -
                GetMessageListView()->GetPreferredSize().height());

  // ScrollView has larger height than MessageListView because it has Clear All
  // button.
  EXPECT_EQ(4 * kUnifiedNotificationCenterSpacing,
            GetScrollerContents()->GetPreferredSize().height() -
                GetMessageListView()->GetPreferredSize().height());

  // When Clear All button is pressed, all notifications are removed and the
  // view becomes invisible.
  message_center_view()->ButtonPressed(nullptr, DummyEvent());
  EXPECT_FALSE(message_center_view()->visible());
}

}  // namespace ash
