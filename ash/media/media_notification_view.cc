// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/media/media_notification_view.h"

#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

MediaNotificationView::MediaNotificationView(
    const message_center::Notification& notification)
    : message_center::MessageView(notification) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(), 0));

  // |controls_button_view_| has the common notification control buttons.
  control_buttons_view_ =
      std::make_unique<message_center::NotificationControlButtonsView>(this);
  control_buttons_view_->set_owned_by_client();

  // |header_row_| contains app_icon, app_name, control buttons, etc...
  header_row_ = new message_center::NotificationHeaderView(
      control_buttons_view_.get(), this);
  header_row_->SetExpandButtonEnabled(false);
  header_row_->SetAppName(
      message_center::MessageCenter::Get()->GetSystemNotificationAppName());
  AddChildView(header_row_);

  // TODO(beccahughes): Add remaining UI for notification.

  UpdateControlButtonsVisibilityWithNotification(notification);
  UpdateCornerRadius(message_center::kNotificationCornerRadius,
                     message_center::kNotificationCornerRadius);
}

MediaNotificationView::~MediaNotificationView() = default;

void MediaNotificationView::UpdateWithNotification(
    const message_center::Notification& notification) {
  MessageView::UpdateWithNotification(notification);

  UpdateControlButtonsVisibilityWithNotification(notification);
  Layout();
  SchedulePaint();
}

message_center::NotificationControlButtonsView*
MediaNotificationView::GetControlButtonsView() const {
  return control_buttons_view_.get();
}

void MediaNotificationView::UpdateControlButtonsVisibility() {
  const bool target_visibility =
      (IsMouseHovered() || control_buttons_view_->IsCloseButtonFocused() ||
       control_buttons_view_->IsSettingsButtonFocused()) &&
      (GetMode() != Mode::SETTING);

  control_buttons_view_->SetVisible(target_visibility);
}

void MediaNotificationView::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
      UpdateControlButtonsVisibility();
      break;
    default:
      break;
  }

  View::OnMouseEvent(event);
}

void MediaNotificationView::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  NOTIMPLEMENTED();
}

void MediaNotificationView::UpdateControlButtonsVisibilityWithNotification(
    const message_center::Notification& notification) {
  // Media notifications do not use the settings and snooze buttons.
  DCHECK(!notification.should_show_settings_button());
  DCHECK(!notification.should_show_snooze_button());

  control_buttons_view_->ShowCloseButton(!notification.pinned());
  UpdateControlButtonsVisibility();
}

}  // namespace ash
