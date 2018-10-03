// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/unified_message_list_view.h"

#include "ash/test/ash_test_base.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/message_view.h"

using message_center::MessageCenter;
using message_center::MessageView;
using message_center::Notification;

namespace ash {

class UnifiedMessageListViewTest : public AshTestBase,
                                   public views::ViewObserver {
 public:
  UnifiedMessageListViewTest() = default;
  ~UnifiedMessageListViewTest() override = default;

  void TearDown() override {
    message_list_view_.reset();
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

  void CreateMessageListView() {
    message_list_view_ = std::make_unique<UnifiedMessageListView>(nullptr);
    message_list_view_->AddObserver(this);
    OnViewPreferredSizeChanged(message_list_view_.get());
    size_changed_count_ = 0;
  }

  MessageView* GetMessageViewAt(int index) const {
    return static_cast<MessageView*>(message_list_view()->child_at(index));
  }

  UnifiedMessageListView* message_list_view() const {
    return message_list_view_.get();
  }

  int size_changed_count() const { return size_changed_count_; }

 private:
  int id_ = 0;
  int size_changed_count_ = 0;

  std::unique_ptr<UnifiedMessageListView> message_list_view_;

  DISALLOW_COPY_AND_ASSIGN(UnifiedMessageListViewTest);
};

TEST_F(UnifiedMessageListViewTest, Open) {
  auto id0 = AddNotification();
  auto id1 = AddNotification();
  auto id2 = AddNotification();
  CreateMessageListView();

  EXPECT_EQ(3, message_list_view()->child_count());
  EXPECT_EQ(id0, GetMessageViewAt(0)->notification_id());
  EXPECT_EQ(id1, GetMessageViewAt(1)->notification_id());
  EXPECT_EQ(id2, GetMessageViewAt(2)->notification_id());

  EXPECT_EQ(GetMessageViewAt(0)->bounds().bottom(),
            GetMessageViewAt(1)->bounds().y());
  EXPECT_EQ(GetMessageViewAt(1)->bounds().bottom(),
            GetMessageViewAt(2)->bounds().y());

  EXPECT_LT(0, message_list_view()->GetPreferredSize().height());
}

TEST_F(UnifiedMessageListViewTest, AddNotifications) {
  CreateMessageListView();
  EXPECT_EQ(0, message_list_view()->GetPreferredSize().height());

  auto id0 = AddNotification();
  EXPECT_EQ(1, size_changed_count());
  EXPECT_EQ(1, message_list_view()->child_count());
  EXPECT_EQ(id0, GetMessageViewAt(0)->notification_id());

  int previous_height = message_list_view()->GetPreferredSize().height();
  EXPECT_LT(0, previous_height);

  gfx::Rect previous_bounds = GetMessageViewAt(0)->bounds();

  auto id1 = AddNotification();
  EXPECT_EQ(2, size_changed_count());
  EXPECT_EQ(2, message_list_view()->child_count());
  EXPECT_EQ(id1, GetMessageViewAt(1)->notification_id());

  EXPECT_LT(previous_height, message_list_view()->GetPreferredSize().height());
  EXPECT_EQ(previous_bounds, GetMessageViewAt(0)->bounds());
  EXPECT_EQ(GetMessageViewAt(0)->bounds().bottom(),
            GetMessageViewAt(1)->bounds().y());
}

TEST_F(UnifiedMessageListViewTest, RemoveNotification) {
  auto id0 = AddNotification();
  auto id1 = AddNotification();

  CreateMessageListView();
  int previous_height = message_list_view()->GetPreferredSize().height();

  gfx::Rect previous_bounds = GetMessageViewAt(0)->bounds();
  MessageCenter::Get()->RemoveNotification(id0, true /* by_user */);
  EXPECT_EQ(1, size_changed_count());
  EXPECT_EQ(previous_bounds.y(), GetMessageViewAt(0)->bounds().y());
  EXPECT_LT(0, message_list_view()->GetPreferredSize().height());
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());

  MessageCenter::Get()->RemoveNotification(id1, true /* by_user */);
  EXPECT_EQ(2, size_changed_count());
  EXPECT_EQ(0, message_list_view()->GetPreferredSize().height());
}

}  // namespace ash
