// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_menu/notification_menu_view.h"

#include "ash/app_menu/notification_item_view.h"
#include "ash/app_menu/notification_menu_header_view.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/proportional_image_view.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

NotificationMenuView::NotificationMenuView(

    NotificationItemView::Delegate* notification_item_view_delegate,
    message_center::SlideOutController::Delegate* slide_out_controller_delegate,
    const std::string& app_id)
    : app_id_(app_id),
      notification_item_view_delegate_(notification_item_view_delegate),
      slide_out_controller_delegate_(slide_out_controller_delegate) {
  DCHECK(notification_item_view_delegate_);
  DCHECK(slide_out_controller_delegate_);
  DCHECK(!app_id_.empty())
      << "Only context menus for applications can show notifications.";

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  double_separator_ =
      new views::MenuSeparator(ui::MenuSeparatorType::DOUBLE_SEPARATOR);
  AddChildView(double_separator_);

  header_view_ = new NotificationMenuHeaderView();
  AddChildView(header_view_);
}

NotificationMenuView::~NotificationMenuView() = default;

bool NotificationMenuView::IsEmpty() const {
  return notification_item_views_.empty();
}

gfx::Size NotificationMenuView::CalculatePreferredSize() const {
  return gfx::Size(views::MenuConfig::instance().touchable_menu_width,
                   double_separator_->GetPreferredSize().height() +
                       header_view_->GetPreferredSize().height() +
                       kNotificationItemViewHeight);
}

void NotificationMenuView::AddNotificationItemView(
    const message_center::Notification& notification) {
  // Remove the displayed NotificationItemView, it is still stored in
  // |notification_item_views_|.
  if (!notification_item_views_.empty())
    RemoveChildView(notification_item_views_.front().get());

  notification_item_views_.push_front(std::make_unique<NotificationItemView>(
      notification_item_view_delegate_, slide_out_controller_delegate_,
      notification.title(), notification.message(), notification.icon(),
      notification.id()));
  notification_item_views_.front()->set_owned_by_client();
  AddChildView(notification_item_views_.front().get());

  header_view_->UpdateCounter(notification_item_views_.size());
}

void NotificationMenuView::UpdateNotificationItemView(
    const message_center::Notification& notification) {
  // Find the view which corresponds to |notification|.
  auto notification_iter = std::find_if(
      notification_item_views_.begin(), notification_item_views_.end(),
      [&notification](
          const std::unique_ptr<NotificationItemView>& notification_item_view) {
        return notification_item_view->notification_id() == notification.id();
      });

  if (notification_iter == notification_item_views_.end())
    return;

  (*notification_iter)
      ->UpdateContents(notification.title(), notification.message(),
                       notification.icon());
}

void NotificationMenuView::RemoveNotificationItemView(
    const std::string& notification_id) {
  // Find the view which corresponds to |notification_id|.
  auto notification_iter = std::find_if(
      notification_item_views_.begin(), notification_item_views_.end(),
      [&notification_id](
          const std::unique_ptr<NotificationItemView>& notification_item_view) {
        return notification_item_view->notification_id() == notification_id;
      });
  if (notification_iter == notification_item_views_.end())
    return;

  notification_item_views_.erase(notification_iter);
  header_view_->UpdateCounter(notification_item_views_.size());

  // Replace the displayed view, if it is already being shown this is a no-op.
  if (!notification_item_views_.empty())
    AddChildView(notification_item_views_.front().get());
}

ui::Layer* NotificationMenuView::GetSlideOutLayer() {
  if (notification_item_views_.empty())
    return nullptr;
  return notification_item_views_.front()->layer();
}

const std::string& NotificationMenuView::GetDisplayedNotificationID() {
  DCHECK(!notification_item_views_.empty());
  return notification_item_views_.front()->notification_id();
}

}  // namespace ash
