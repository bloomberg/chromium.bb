// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/session/tray_session_length_limit.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/session_length_limit_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/label_tray_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/views/view.h"

namespace ash {
namespace {

const char kNotifierSessionLengthTimeout[] = "ash.session-length-timeout";

// A notification is shown to the user only if the remaining session time falls
// under this threshold. e.g. If the user has several days left in their
// session, there is no use displaying a notification right now.
constexpr base::TimeDelta kNotificationThreshold =
    base::TimeDelta::FromMinutes(60);

}  // namespace

// static
const char TraySessionLengthLimit::kNotificationId[] =
    "chrome://session/timeout";

TraySessionLengthLimit::TraySessionLengthLimit(SystemTray* system_tray)
    : SystemTrayItem(system_tray, UMA_SESSION_LENGTH_LIMIT),
      model_(Shell::Get()->system_tray_model()->session_length_limit()) {
  model_->AddObserver(this);
  OnSessionLengthLimitUpdated();
}

TraySessionLengthLimit::~TraySessionLengthLimit() {
  model_->RemoveObserver(this);
}

// Add view to tray bubble.
views::View* TraySessionLengthLimit::CreateDefaultView(LoginStatus status) {
  CHECK(!tray_bubble_view_);
  if (model_->limit_state() == SessionLengthLimitModel::LIMIT_NONE)
    return nullptr;
  tray_bubble_view_ = new LabelTrayView(nullptr, kSystemMenuTimerIcon);
  tray_bubble_view_->SetMessage(ComposeTrayBubbleMessage());
  return tray_bubble_view_;
}

// View has been removed from tray bubble.
void TraySessionLengthLimit::OnDefaultViewDestroyed() {
  tray_bubble_view_ = nullptr;
}

void TraySessionLengthLimit::OnSessionLengthLimitUpdated() {
  // Don't show notification or tray item until the user is logged in.
  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted())
    return;

  UpdateNotification();
  UpdateTrayBubbleView();
  last_limit_state_ = model_->limit_state();
}

void TraySessionLengthLimit::UpdateNotification() {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();

  // If state hasn't changed and the notification has already been acknowledged,
  // we won't re-create it. We consider a notification to be acknowledged if it
  // was shown before, but is no longer visible.
  if (model_->limit_state() == last_limit_state_ &&
      has_notification_been_shown_ &&
      !message_center->FindVisibleNotificationById(kNotificationId)) {
    return;
  }

  // After state change, any possibly existing notification is removed to make
  // sure it is re-shown even if it had been acknowledged by the user before
  // (and in the rare case of state change towards LIMIT_NONE to make the
  // notification disappear).
  if (model_->limit_state() != last_limit_state_ &&
      message_center->FindVisibleNotificationById(kNotificationId)) {
    message_center::MessageCenter::Get()->RemoveNotification(
        kNotificationId, false /* by_user */);
  }

  // If the session is unlimited or if the remaining time is too far off into
  // the future, there is nothing more to do.
  if (model_->limit_state() == SessionLengthLimitModel::LIMIT_NONE ||
      model_->remaining_session_time() > kNotificationThreshold) {
    return;
  }

  message_center::RichNotificationData data;
  data.should_make_spoken_feedback_for_popup_updates =
      (model_->limit_state() != last_limit_state_);
  std::unique_ptr<message_center::Notification> notification =
      message_center::Notification::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
          ComposeNotificationTitle(),
          l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_NOTIFICATION_SESSION_LENGTH_LIMIT_MESSAGE),
          gfx::Image(), base::string16() /* display_source */, GURL(),
          message_center::NotifierId(
              message_center::NotifierId::SYSTEM_COMPONENT,
              kNotifierSessionLengthTimeout),
          data, nullptr /* delegate */, kNotificationTimerIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  notification->SetSystemPriority();
  if (message_center->FindVisibleNotificationById(kNotificationId)) {
    message_center->UpdateNotification(kNotificationId,
                                       std::move(notification));
  } else {
    message_center->AddNotification(std::move(notification));
  }
  has_notification_been_shown_ = true;
}

void TraySessionLengthLimit::UpdateTrayBubbleView() const {
  if (!tray_bubble_view_)
    return;
  if (model_->limit_state() == SessionLengthLimitModel::LIMIT_NONE)
    tray_bubble_view_->SetMessage(base::string16());
  else
    tray_bubble_view_->SetMessage(ComposeTrayBubbleMessage());
  tray_bubble_view_->Layout();
}

base::string16 TraySessionLengthLimit::ComposeNotificationTitle() const {
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NOTIFICATION_SESSION_LENGTH_LIMIT_TITLE,
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                             ui::TimeFormat::LENGTH_SHORT,
                             model_->remaining_session_time()));
}

base::string16 TraySessionLengthLimit::ComposeTrayBubbleMessage() const {
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BUBBLE_SESSION_LENGTH_LIMIT,
      ui::TimeFormat::Detailed(ui::TimeFormat::FORMAT_DURATION,
                               ui::TimeFormat::LENGTH_LONG, 10,
                               model_->remaining_session_time()));
}

}  // namespace ash
