// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/unified_message_list_view.h"

#include "ash/system/tray/tray_constants.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/notification_view_md.h"

using message_center::MessageCenter;
using message_center::MessageView;
using message_center::Notification;

namespace ash {

namespace {

class TestNotificationView : public message_center::NotificationViewMD {
 public:
  TestNotificationView(const message_center::Notification& notification)
      : NotificationViewMD(notification) {}
  ~TestNotificationView() override = default;

  // message_center::NotificationViewMD:
  void UpdateCornerRadius(int top_radius, int bottom_radius) override {
    top_radius_ = top_radius;
    bottom_radius_ = bottom_radius;
    message_center::NotificationViewMD::UpdateCornerRadius(top_radius,
                                                           bottom_radius);
  }

  int top_radius() const { return top_radius_; }
  int bottom_radius() const { return bottom_radius_; }

 private:
  int top_radius_ = 0;
  int bottom_radius_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestNotificationView);
};

class TestUnifiedMessageListView : public UnifiedMessageListView {
 public:
  TestUnifiedMessageListView() : UnifiedMessageListView(nullptr) {}

  ~TestUnifiedMessageListView() override = default;

  // UnifiedMessageListView:
  message_center::MessageView* CreateMessageView(
      const message_center::Notification& notification) override {
    auto* view = new TestNotificationView(notification);
    view->SetIsNested();
    return view;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestUnifiedMessageListView);
};

}  // namespace

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
    message_list_view_ = std::make_unique<TestUnifiedMessageListView>();
    message_list_view_->Init();
    message_list_view_->AddObserver(this);
    OnViewPreferredSizeChanged(message_list_view_.get());
    size_changed_count_ = 0;
  }

  TestNotificationView* GetMessageViewAt(int index) const {
    return static_cast<TestNotificationView*>(
        message_list_view()->child_at(index)->child_at(1));
  }

  gfx::Rect GetMessageViewBounds(int index) const {
    return message_list_view()->child_at(index)->bounds();
  }

  UnifiedMessageListView* message_list_view() const {
    return message_list_view_.get();
  }

  int size_changed_count() const { return size_changed_count_; }

 private:
  int id_ = 0;
  int size_changed_count_ = 0;

  std::unique_ptr<TestUnifiedMessageListView> message_list_view_;

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

  EXPECT_FALSE(GetMessageViewAt(0)->IsExpanded());
  EXPECT_FALSE(GetMessageViewAt(1)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(2)->IsExpanded());

  EXPECT_EQ(GetMessageViewBounds(0).bottom(), GetMessageViewBounds(1).y());
  EXPECT_EQ(GetMessageViewBounds(1).bottom(), GetMessageViewBounds(2).y());

  EXPECT_EQ(kUnifiedTrayCornerRadius, GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(0, GetMessageViewAt(1)->top_radius());
  EXPECT_EQ(0, GetMessageViewAt(2)->top_radius());

  EXPECT_EQ(0, GetMessageViewAt(0)->bottom_radius());
  EXPECT_EQ(0, GetMessageViewAt(1)->bottom_radius());
  EXPECT_EQ(kUnifiedTrayCornerRadius, GetMessageViewAt(2)->bottom_radius());

  EXPECT_LT(0, message_list_view()->GetPreferredSize().height());
}

TEST_F(UnifiedMessageListViewTest, AddNotifications) {
  CreateMessageListView();
  EXPECT_EQ(0, message_list_view()->GetPreferredSize().height());

  auto id0 = AddNotification();
  EXPECT_EQ(1, size_changed_count());
  EXPECT_EQ(1, message_list_view()->child_count());
  EXPECT_EQ(id0, GetMessageViewAt(0)->notification_id());

  EXPECT_EQ(kUnifiedTrayCornerRadius, GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(kUnifiedTrayCornerRadius, GetMessageViewAt(0)->bottom_radius());

  int previous_height = message_list_view()->GetPreferredSize().height();
  EXPECT_LT(0, previous_height);

  gfx::Rect previous_bounds = GetMessageViewBounds(0);

  auto id1 = AddNotification();
  EXPECT_EQ(2, size_changed_count());
  EXPECT_EQ(2, message_list_view()->child_count());
  EXPECT_EQ(id1, GetMessageViewAt(1)->notification_id());

  EXPECT_LT(previous_height, message_list_view()->GetPreferredSize().height());
  // 1dip larger because now it has separator border.
  previous_bounds.Inset(gfx::Insets(0, 0, -1, 0));
  EXPECT_EQ(previous_bounds, GetMessageViewBounds(0));
  EXPECT_EQ(GetMessageViewBounds(0).bottom(), GetMessageViewBounds(1).y());

  EXPECT_EQ(kUnifiedTrayCornerRadius, GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(0, GetMessageViewAt(1)->top_radius());

  EXPECT_EQ(0, GetMessageViewAt(0)->bottom_radius());
  EXPECT_EQ(kUnifiedTrayCornerRadius, GetMessageViewAt(1)->bottom_radius());
}

TEST_F(UnifiedMessageListViewTest, RemoveNotification) {
  auto id0 = AddNotification();
  auto id1 = AddNotification();

  CreateMessageListView();
  int previous_height = message_list_view()->GetPreferredSize().height();

  EXPECT_EQ(kUnifiedTrayCornerRadius, GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(0, GetMessageViewAt(0)->bottom_radius());

  gfx::Rect previous_bounds = GetMessageViewBounds(0);
  MessageCenter::Get()->RemoveNotification(id0, true /* by_user */);
  EXPECT_EQ(1, size_changed_count());
  EXPECT_EQ(previous_bounds.y(), GetMessageViewBounds(0).y());
  EXPECT_LT(0, message_list_view()->GetPreferredSize().height());
  EXPECT_GT(previous_height, message_list_view()->GetPreferredSize().height());

  EXPECT_EQ(kUnifiedTrayCornerRadius, GetMessageViewAt(0)->top_radius());
  EXPECT_EQ(kUnifiedTrayCornerRadius, GetMessageViewAt(0)->bottom_radius());

  MessageCenter::Get()->RemoveNotification(id1, true /* by_user */);
  EXPECT_EQ(2, size_changed_count());
  EXPECT_EQ(0, message_list_view()->GetPreferredSize().height());
}

TEST_F(UnifiedMessageListViewTest, CollapseOlderNotifications) {
  AddNotification();
  CreateMessageListView();
  EXPECT_TRUE(GetMessageViewAt(0)->IsExpanded());

  AddNotification();
  EXPECT_FALSE(GetMessageViewAt(0)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(1)->IsExpanded());

  AddNotification();
  EXPECT_FALSE(GetMessageViewAt(0)->IsExpanded());
  EXPECT_FALSE(GetMessageViewAt(1)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(2)->IsExpanded());

  GetMessageViewAt(1)->SetExpanded(true);
  GetMessageViewAt(1)->SetManuallyExpandedOrCollapsed(true);

  AddNotification();
  EXPECT_FALSE(GetMessageViewAt(0)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(1)->IsExpanded());
  EXPECT_FALSE(GetMessageViewAt(2)->IsExpanded());
  EXPECT_TRUE(GetMessageViewAt(3)->IsExpanded());
}

}  // namespace ash
