// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/session/tray_session_length_limit.h"

#include <algorithm>

#include "ash/shell.h"
#include "ash/system/chromeos/label_tray_view.h"
#include "ash/system/system_notifier.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/views/view.h"

namespace ash {
namespace {

// If the remaining session time falls below this threshold, the user should be
// informed that the session is about to expire.
const int kExpiringSoonThresholdInMinutes = 5;

// Use 500ms interval for updates to notification and tray bubble to reduce the
// likelihood of a user-visible skip in high load situations (as might happen
// with 1000ms).
const int kTimerIntervalInMilliseconds = 500;

}  // namespace

// static
const char TraySessionLengthLimit::kNotificationId[] =
    "chrome://session/timeout";

TraySessionLengthLimit::TraySessionLengthLimit(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      limit_state_(LIMIT_NONE),
      last_limit_state_(LIMIT_NONE),
      tray_bubble_view_(NULL) {
  Shell::GetInstance()->system_tray_notifier()->
      AddSessionLengthLimitObserver(this);
  Update();
}

TraySessionLengthLimit::~TraySessionLengthLimit() {
  Shell::GetInstance()->system_tray_notifier()->
      RemoveSessionLengthLimitObserver(this);
}

// Add view to tray bubble.
views::View* TraySessionLengthLimit::CreateDefaultView(
    user::LoginStatus status) {
  CHECK(!tray_bubble_view_);
  UpdateState();
  if (limit_state_ == LIMIT_NONE)
    return NULL;
  tray_bubble_view_ = new LabelTrayView(
      NULL /* click_listener */,
      IDR_AURA_UBER_TRAY_BUBBLE_SESSION_LENGTH_LIMIT);
  tray_bubble_view_->SetMessage(ComposeTrayBubbleMessage());
  return tray_bubble_view_;
}

// View has been removed from tray bubble.
void TraySessionLengthLimit::DestroyDefaultView() {
  tray_bubble_view_ = NULL;
}

void TraySessionLengthLimit::OnSessionStartTimeChanged() {
  Update();
}

void TraySessionLengthLimit::OnSessionLengthLimitChanged() {
  Update();
}

void TraySessionLengthLimit::Update() {
  UpdateState();
  UpdateNotification();
  UpdateTrayBubbleView();
}

void TraySessionLengthLimit::UpdateState() {
  SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();
  if (delegate->GetSessionStartTime(&session_start_time_) &&
      delegate->GetSessionLengthLimit(&time_limit_)) {
    const base::TimeDelta expiring_soon_threshold(
        base::TimeDelta::FromMinutes(kExpiringSoonThresholdInMinutes));
    remaining_session_time_ = std::max(
        time_limit_ - (base::TimeTicks::Now() - session_start_time_),
        base::TimeDelta());
    limit_state_ = remaining_session_time_ <= expiring_soon_threshold ?
        LIMIT_EXPIRING_SOON : LIMIT_SET;
    if (!timer_)
      timer_.reset(new base::RepeatingTimer<TraySessionLengthLimit>);
    if (!timer_->IsRunning()) {
      timer_->Start(FROM_HERE,
                    base::TimeDelta::FromMilliseconds(
                        kTimerIntervalInMilliseconds),
                    this,
                    &TraySessionLengthLimit::Update);
    }
  } else {
    remaining_session_time_ = base::TimeDelta();
    limit_state_ = LIMIT_NONE;
    timer_.reset();
  }
}

void TraySessionLengthLimit::UpdateNotification() {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();

  // If state hasn't changed and the notification has already been acknowledged,
  // we won't re-create it.
  if (limit_state_ == last_limit_state_ &&
      !message_center->FindVisibleNotificationById(kNotificationId)) {
    return;
  }

  // After state change, any possibly existing notification is removed to make
  // sure it is re-shown even if it had been acknowledged by the user before
  // (and in the rare case of state change towards LIMIT_NONE to make the
  // notification disappear).
  if (limit_state_ != last_limit_state_ &&
      message_center->FindVisibleNotificationById(kNotificationId)) {
    message_center::MessageCenter::Get()->RemoveNotification(
        kNotificationId, false /* by_user */);
  }

  // For LIMIT_NONE, there's nothing more to do.
  if (limit_state_ == LIMIT_NONE) {
    last_limit_state_ = limit_state_;
    return;
  }

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  message_center::RichNotificationData data;
  data.should_make_spoken_feedback_for_popup_updates =
      (limit_state_ != last_limit_state_);
  scoped_ptr<message_center::Notification> notification(
      new message_center::Notification(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kNotificationId,
          base::string16() /* title */,
          ComposeNotificationMessage() /* message */,
          bundle.GetImageNamed(
              IDR_AURA_UBER_TRAY_NOTIFICATION_SESSION_LENGTH_LIMIT),
          base::string16() /* display_source */,
          message_center::NotifierId(
              message_center::NotifierId::SYSTEM_COMPONENT,
              system_notifier::kNotifierSessionLengthTimeout),
          data,
          NULL /* delegate */));
  notification->SetSystemPriority();
  if (message_center->FindVisibleNotificationById(kNotificationId))
    message_center->UpdateNotification(kNotificationId, notification.Pass());
  else
    message_center->AddNotification(notification.Pass());
  last_limit_state_ = limit_state_;
}

void TraySessionLengthLimit::UpdateTrayBubbleView() const {
  if (!tray_bubble_view_)
    return;
  if (limit_state_ == LIMIT_NONE)
    tray_bubble_view_->SetMessage(base::string16());
  else
    tray_bubble_view_->SetMessage(ComposeTrayBubbleMessage());
  tray_bubble_view_->Layout();
}

base::string16 TraySessionLengthLimit::ComposeNotificationMessage() const {
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NOTIFICATION_SESSION_LENGTH_LIMIT,
      ui::TimeFormat::Detailed(ui::TimeFormat::FORMAT_DURATION,
                               ui::TimeFormat::LENGTH_LONG,
                               10,
                               remaining_session_time_));
}

base::string16 TraySessionLengthLimit::ComposeTrayBubbleMessage() const {
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BUBBLE_SESSION_LENGTH_LIMIT,
      ui::TimeFormat::Detailed(ui::TimeFormat::FORMAT_DURATION,
                               ui::TimeFormat::LENGTH_LONG,
                               10,
                               remaining_session_time_));
}

}  // namespace ash
