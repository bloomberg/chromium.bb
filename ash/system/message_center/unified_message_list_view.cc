// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/unified_message_list_view.h"

#include "ash/system/message_center/new_unified_message_center_view.h"
#include "base/auto_reset.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/message_view_factory.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"

using message_center::Notification;
using message_center::MessageCenter;
using message_center::MessageView;

namespace ash {

UnifiedMessageListView::UnifiedMessageListView(
    NewUnifiedMessageCenterView* message_center_view)
    : message_center_view_(message_center_view) {
  MessageCenter::Get()->AddObserver(this);

  SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));

  bool is_latest = true;
  for (auto* notification : MessageCenter::Get()->GetVisibleNotifications()) {
    auto* view = CreateMessageView(*notification);
    // Expand the latest notification, and collapse all other notifications.
    view->SetExpanded(is_latest && view->IsAutoExpandingAllowed());
    is_latest = false;
    AddChildViewAt(view, 0);
    MessageCenter::Get()->DisplayedNotification(
        notification->id(), message_center::DISPLAY_SOURCE_MESSAGE_CENTER);
  }
}

UnifiedMessageListView::~UnifiedMessageListView() {
  MessageCenter::Get()->RemoveObserver(this);
}

void UnifiedMessageListView::ChildPreferredSizeChanged(views::View* child) {
  if (ignore_size_change_)
    return;
  PreferredSizeChanged();
}

void UnifiedMessageListView::PreferredSizeChanged() {
  views::View::PreferredSizeChanged();
  if (message_center_view_)
    message_center_view_->ListPreferredSizeChanged();
}

void UnifiedMessageListView::OnNotificationAdded(const std::string& id) {
  auto* notification = MessageCenter::Get()->FindVisibleNotificationById(id);
  if (!notification)
    return;

  // Collapse all notifications before adding new one.
  CollapseAllNotifications();

  auto* view = CreateMessageView(*notification);
  // Expand the latest notification.
  view->SetExpanded(view->IsAutoExpandingAllowed());
  AddChildView(view);
  PreferredSizeChanged();
}

void UnifiedMessageListView::OnNotificationRemoved(const std::string& id,
                                                   bool by_user) {
  for (int i = 0; i < child_count(); ++i) {
    auto* view = static_cast<message_center::MessageView*>(child_at(i));
    if (view->notification_id() == id) {
      delete view;
      break;
    }
  }

  PreferredSizeChanged();
}

void UnifiedMessageListView::OnNotificationUpdated(const std::string& id) {
  auto* notification = MessageCenter::Get()->FindVisibleNotificationById(id);
  if (!notification)
    return;

  for (int i = 0; i < child_count(); ++i) {
    auto* view = static_cast<message_center::MessageView*>(child_at(i));
    if (view->notification_id() == id) {
      view->UpdateWithNotification(*notification);
      break;
    }
  }

  PreferredSizeChanged();
}

message_center::MessageView* UnifiedMessageListView::CreateMessageView(
    const Notification& notification) const {
  auto* view = message_center::MessageViewFactory::Create(notification);
  view->SetIsNested();
  view->UpdateCornerRadius(0, 0);
  view->SetBorder(views::NullBorder());
  if (message_center_view_)
    message_center_view_->ConfigureMessageView(view);
  return view;
}

void UnifiedMessageListView::CollapseAllNotifications() {
  base::AutoReset<bool> auto_reset(&ignore_size_change_, true);
  for (int i = 0; i < child_count(); ++i) {
    auto* view = static_cast<message_center::MessageView*>(child_at(i));
    if (!view->IsManuallyExpandedOrCollapsed())
      view->SetExpanded(false);
  }
}

}  // namespace ash
