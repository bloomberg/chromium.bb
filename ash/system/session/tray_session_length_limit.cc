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
#include "ui/message_center/notification.h"
#include "ui/views/view.h"

namespace ash {
namespace {

const char kNotifierSessionLengthTimeout[] = "ash.session-length-timeout";

// If the remaining session time falls below this threshold, the user should be
// informed that the session is about to expire.
const int kExpiringSoonThresholdInMinutes = 5;

// A notification is shown to the user only if the remaining session time falls
// under this threshold. e.g. If the user has several days left in their
// session, there is no use displaying a notification right now.
constexpr base::TimeDelta kNotificationThreshold =
    base::TimeDelta::FromMinutes(60);

// Use 500ms interval for updates to notification and tray bubble to reduce the
// likelihood of a user-visible skip in high load situations (as might happen
// with 1000ms).
const int kTimerIntervalInMilliseconds = 500;

}  // namespace

// static
const char TraySessionLengthLimit::kNotificationId[] =
    "chrome://session/timeout";

TraySessionLengthLimit::TraySessionLengthLimit(SystemTray* system_tray)
    : SystemTrayItem(system_tray, UMA_SESSION_LENGTH_LIMIT),
      limit_state_(LIMIT_NONE),
      last_limit_state_(LIMIT_NONE),
      has_notification_been_shown_(false),
      tray_bubble_view_(nullptr) {
  Shell::Get()->session_controller()->AddObserver(this);
  Update();
}

TraySessionLengthLimit::~TraySessionLengthLimit() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

// Add view to tray bubble.
views::View* TraySessionLengthLimit::CreateDefaultView(LoginStatus status) {
  CHECK(!tray_bubble_view_);
  UpdateState();
  if (limit_state_ == LIMIT_NONE)
    return nullptr;
  tray_bubble_view_ = new LabelTrayView(nullptr, kSystemMenuTimerIcon);
  tray_bubble_view_->SetMessage(ComposeTrayBubbleMessage());
  return tray_bubble_view_;
}

// View has been removed from tray bubble.
void TraySessionLengthLimit::OnDefaultViewDestroyed() {
  tray_bubble_view_ = nullptr;
}

void TraySessionLengthLimit::OnSessionStateChanged(
    session_manager::SessionState state) {
  Update();
}

void TraySessionLengthLimit::OnSessionLengthLimitChanged() {
  Update();
}

void TraySessionLengthLimit::Update() {
  // Don't show notification or tray item until the user is logged in.
  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted())
    return;

  UpdateState();
  UpdateNotification();
  UpdateTrayBubbleView();
}

void TraySessionLengthLimit::UpdateState() {
  SessionController* session = Shell::Get()->session_controller();
  base::TimeDelta time_limit = session->session_length_limit();
  base::TimeTicks session_start_time = session->session_start_time();
  if (!time_limit.is_zero() && !session_start_time.is_null()) {
    const base::TimeDelta expiring_soon_threshold(
        base::TimeDelta::FromMinutes(kExpiringSoonThresholdInMinutes));
    remaining_session_time_ =
        std::max(time_limit - (base::TimeTicks::Now() - session_start_time),
                 base::TimeDelta());
    limit_state_ = remaining_session_time_ <= expiring_soon_threshold
                       ? LIMIT_EXPIRING_SOON
                       : LIMIT_SET;
    if (!timer_)
      timer_.reset(new base::RepeatingTimer);
    if (!timer_->IsRunning()) {
      timer_->Start(FROM_HERE, base::TimeDelta::FromMilliseconds(
                                   kTimerIntervalInMilliseconds),
                    this, &TraySessionLengthLimit::Update);
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
  // we won't re-create it. We consider a notification to be acknowledged if it
  // was shown before, but is no longer visible.
  if (limit_state_ == last_limit_state_ && has_notification_been_shown_ &&
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

  // If the session is unlimited or if the remaining time is too far off into
  // the future, there is nothing more to do.
  if (limit_state_ == LIMIT_NONE ||
      remaining_session_time_ > kNotificationThreshold) {
    last_limit_state_ = limit_state_;
    return;
  }

  message_center::RichNotificationData data;
  data.should_make_spoken_feedback_for_popup_updates =
      (limit_state_ != last_limit_state_);
  std::unique_ptr<message_center::Notification> notification =
      message_center::Notification::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kNotificationId,
          base::string16() /* title */,
          ComposeNotificationMessage() /* message */, gfx::Image(),
          base::string16() /* display_source */, GURL(),
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
                               ui::TimeFormat::LENGTH_LONG, 10,
                               remaining_session_time_));
}

base::string16 TraySessionLengthLimit::ComposeTrayBubbleMessage() const {
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BUBBLE_SESSION_LENGTH_LIMIT,
      ui::TimeFormat::Detailed(ui::TimeFormat::FORMAT_DURATION,
                               ui::TimeFormat::LENGTH_LONG, 10,
                               remaining_session_time_));
}

}  // namespace ash
