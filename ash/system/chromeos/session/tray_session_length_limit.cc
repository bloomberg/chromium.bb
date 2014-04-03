// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/session/tray_session_length_limit.h"

#include <algorithm>

#include "ash/shelf/shelf_types.h"
#include "ash/shell.h"
#include "ash/system/system_notifier.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font_list.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

using message_center::Notification;

namespace ash {
namespace {

// If the remaining session time falls below this threshold, the user should be
// informed that the session is about to expire.
const int kExpiringSoonThresholdInSeconds = 5 * 60;  // 5 minutes.

// Color in which the remaining session time is normally shown.
const SkColor kRemainingTimeColor = SK_ColorWHITE;
// Color in which the remaining session time is shown when it is expiring soon.
const SkColor kRemainingTimeExpiringSoonColor = SK_ColorRED;

views::Label* CreateAndSetupLabel() {
  views::Label* label = new views::Label;
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  SetupLabelForTray(label);
  label->SetFontList(label->font_list().DeriveWithStyle(
      label->font_list().GetFontStyle() & ~gfx::Font::BOLD));
  return label;
}

base::string16 IntToTwoDigitString(int value) {
  DCHECK_GE(value, 0);
  DCHECK_LE(value, 99);
  if (value < 10)
    return base::ASCIIToUTF16("0") + base::IntToString16(value);
  return base::IntToString16(value);
}

base::string16 FormatRemainingSessionTimeNotification(
    const base::TimeDelta& remaining_session_time) {
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_REMAINING_SESSION_TIME_NOTIFICATION,
      ui::TimeFormat::Detailed(ui::TimeFormat::FORMAT_DURATION,
                               ui::TimeFormat::LENGTH_LONG,
                               10,
                               remaining_session_time));
}

// Creates, or updates the notification for session length timeout with
// |remaining_time|.  |state_changed| is true when its internal state has been
// changed from another.
void CreateOrUpdateNotification(const std::string& notification_id,
                                const base::TimeDelta& remaining_time,
                                bool state_changed) {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();

  // Do not create a new notification if no state has changed. It may happen
  // when the notification is already closed by the user, see crbug.com/285941.
  if (!state_changed && !message_center->HasNotification(notification_id))
    return;

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  message_center::RichNotificationData data;
  // Makes the spoken feedback only when the state has been changed.
  data.should_make_spoken_feedback_for_popup_updates = state_changed;
  scoped_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      notification_id,
      FormatRemainingSessionTimeNotification(remaining_time),
      base::string16() /* message */,
      bundle.GetImageNamed(IDR_AURA_UBER_TRAY_SESSION_LENGTH_LIMIT_TIMER),
      base::string16() /* display_source */,
      message_center::NotifierId(
          message_center::NotifierId::SYSTEM_COMPONENT,
          system_notifier::kNotifierSessionLengthTimeout),
      data,
      NULL /* delegate */));
  notification->SetSystemPriority();
  message_center::MessageCenter::Get()->AddNotification(notification.Pass());
}

}  // namespace

namespace tray {

class RemainingSessionTimeTrayView : public views::View {
 public:
  RemainingSessionTimeTrayView(const TraySessionLengthLimit* owner,
                               ShelfAlignment shelf_alignment);
  virtual ~RemainingSessionTimeTrayView();

  void UpdateClockLayout(ShelfAlignment shelf_alignment);
  void Update();

 private:
  void SetBorderFromAlignment(ShelfAlignment shelf_alignment);

  const TraySessionLengthLimit* owner_;

  views::Label* horizontal_layout_label_;
  views::Label* vertical_layout_label_hours_left_;
  views::Label* vertical_layout_label_hours_right_;
  views::Label* vertical_layout_label_minutes_left_;
  views::Label* vertical_layout_label_minutes_right_;
  views::Label* vertical_layout_label_seconds_left_;
  views::Label* vertical_layout_label_seconds_right_;

  DISALLOW_COPY_AND_ASSIGN(RemainingSessionTimeTrayView);
};

RemainingSessionTimeTrayView::RemainingSessionTimeTrayView(
    const TraySessionLengthLimit* owner,
    ShelfAlignment shelf_alignment)
    : owner_(owner),
      horizontal_layout_label_(NULL),
      vertical_layout_label_hours_left_(NULL),
      vertical_layout_label_hours_right_(NULL),
      vertical_layout_label_minutes_left_(NULL),
      vertical_layout_label_minutes_right_(NULL),
      vertical_layout_label_seconds_left_(NULL),
      vertical_layout_label_seconds_right_(NULL) {
  UpdateClockLayout(shelf_alignment);
}

RemainingSessionTimeTrayView::~RemainingSessionTimeTrayView() {
}

void RemainingSessionTimeTrayView::UpdateClockLayout(
    ShelfAlignment shelf_alignment) {
  SetBorderFromAlignment(shelf_alignment);
  const bool horizontal_layout = (shelf_alignment == SHELF_ALIGNMENT_BOTTOM ||
      shelf_alignment == SHELF_ALIGNMENT_TOP);
  if (horizontal_layout && !horizontal_layout_label_) {
    // Remove labels used for vertical layout.
    RemoveAllChildViews(true);
    vertical_layout_label_hours_left_ = NULL;
    vertical_layout_label_hours_right_ = NULL;
    vertical_layout_label_minutes_left_ = NULL;
    vertical_layout_label_minutes_right_ = NULL;
    vertical_layout_label_seconds_left_ = NULL;
    vertical_layout_label_seconds_right_ = NULL;

    // Create label used for horizontal layout.
    horizontal_layout_label_ = CreateAndSetupLabel();

    // Construct layout.
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
    AddChildView(horizontal_layout_label_);

  } else if (!horizontal_layout && horizontal_layout_label_) {
    // Remove label used for horizontal layout.
    RemoveAllChildViews(true);
    horizontal_layout_label_ = NULL;

    // Create labels used for vertical layout.
    vertical_layout_label_hours_left_ = CreateAndSetupLabel();
    vertical_layout_label_hours_right_ = CreateAndSetupLabel();
    vertical_layout_label_minutes_left_ = CreateAndSetupLabel();
    vertical_layout_label_minutes_right_ = CreateAndSetupLabel();
    vertical_layout_label_seconds_left_ = CreateAndSetupLabel();
    vertical_layout_label_seconds_right_ = CreateAndSetupLabel();

    // Construct layout.
    views::GridLayout* layout = new views::GridLayout(this);
    SetLayoutManager(layout);
    views::ColumnSet* columns = layout->AddColumnSet(0);
    columns->AddPaddingColumn(0, 6);
    columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                       0, views::GridLayout::USE_PREF, 0, 0);
    columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER,
                       0, views::GridLayout::USE_PREF, 0, 0);
    layout->AddPaddingRow(0, kTrayLabelItemVerticalPaddingVerticalAlignment);
    layout->StartRow(0, 0);
    layout->AddView(vertical_layout_label_hours_left_);
    layout->AddView(vertical_layout_label_hours_right_);
    layout->StartRow(0, 0);
    layout->AddView(vertical_layout_label_minutes_left_);
    layout->AddView(vertical_layout_label_minutes_right_);
    layout->StartRow(0, 0);
    layout->AddView(vertical_layout_label_seconds_left_);
    layout->AddView(vertical_layout_label_seconds_right_);
    layout->AddPaddingRow(0, kTrayLabelItemVerticalPaddingVerticalAlignment);
  }
  Update();
}

void RemainingSessionTimeTrayView::Update() {
  const TraySessionLengthLimit::LimitState limit_state =
      owner_->GetLimitState();

  if (limit_state == TraySessionLengthLimit::LIMIT_NONE) {
    SetVisible(false);
    return;
  }

  int seconds = owner_->GetRemainingSessionTime().InSeconds();
  // If the remaining session time is 100 hours or more, show 99:59:59 instead.
  seconds = std::min(seconds, 100 * 60 * 60 - 1);  // 100 hours - 1 second.
  int minutes = seconds / 60;
  seconds %= 60;
  const int hours = minutes / 60;
  minutes %= 60;

  const base::string16 hours_str = IntToTwoDigitString(hours);
  const base::string16 minutes_str = IntToTwoDigitString(minutes);
  const base::string16 seconds_str = IntToTwoDigitString(seconds);
  const SkColor color =
      limit_state == TraySessionLengthLimit::LIMIT_EXPIRING_SOON ?
          kRemainingTimeExpiringSoonColor : kRemainingTimeColor;

  if (horizontal_layout_label_) {
    horizontal_layout_label_->SetText(l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_REMAINING_SESSION_TIME,
        hours_str, minutes_str, seconds_str));
    horizontal_layout_label_->SetEnabledColor(color);
  } else if (vertical_layout_label_hours_left_) {
    vertical_layout_label_hours_left_->SetText(hours_str.substr(0, 1));
    vertical_layout_label_hours_right_->SetText(hours_str.substr(1, 1));
    vertical_layout_label_minutes_left_->SetText(minutes_str.substr(0, 1));
    vertical_layout_label_minutes_right_->SetText(minutes_str.substr(1, 1));
    vertical_layout_label_seconds_left_->SetText(seconds_str.substr(0, 1));
    vertical_layout_label_seconds_right_->SetText(seconds_str.substr(1, 1));
    vertical_layout_label_hours_left_->SetEnabledColor(color);
    vertical_layout_label_hours_right_->SetEnabledColor(color);
    vertical_layout_label_minutes_left_->SetEnabledColor(color);
    vertical_layout_label_minutes_right_->SetEnabledColor(color);
    vertical_layout_label_seconds_left_->SetEnabledColor(color);
    vertical_layout_label_seconds_right_->SetEnabledColor(color);
  }

  Layout();
  SetVisible(true);
}

void RemainingSessionTimeTrayView::SetBorderFromAlignment(
    ShelfAlignment shelf_alignment) {
  if (shelf_alignment == SHELF_ALIGNMENT_BOTTOM ||
      shelf_alignment == SHELF_ALIGNMENT_TOP) {
    SetBorder(views::Border::CreateEmptyBorder(
        0,
        kTrayLabelItemHorizontalPaddingBottomAlignment,
        0,
        kTrayLabelItemHorizontalPaddingBottomAlignment));
  } else {
    SetBorder(views::Border::NullBorder());
  }
}

}  // namespace tray

// static
const char TraySessionLengthLimit::kNotificationId[] =
    "chrome://session/timeout";

TraySessionLengthLimit::TraySessionLengthLimit(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      tray_view_(NULL),
      limit_state_(LIMIT_NONE) {
  Shell::GetInstance()->system_tray_notifier()->
      AddSessionLengthLimitObserver(this);
  Update();
}

TraySessionLengthLimit::~TraySessionLengthLimit() {
  Shell::GetInstance()->system_tray_notifier()->
      RemoveSessionLengthLimitObserver(this);
}

views::View* TraySessionLengthLimit::CreateTrayView(user::LoginStatus status) {
  CHECK(tray_view_ == NULL);
  tray_view_ = new tray::RemainingSessionTimeTrayView(
      this, system_tray()->shelf_alignment());
  return tray_view_;
}

void TraySessionLengthLimit::DestroyTrayView() {
  tray_view_ = NULL;
}

void TraySessionLengthLimit::UpdateAfterShelfAlignmentChange(
    ShelfAlignment alignment) {
  if (tray_view_)
    tray_view_->UpdateClockLayout(alignment);
}

void TraySessionLengthLimit::OnSessionStartTimeChanged() {
  Update();
}

void TraySessionLengthLimit::OnSessionLengthLimitChanged() {
  Update();
}

TraySessionLengthLimit::LimitState
    TraySessionLengthLimit::GetLimitState() const {
  return limit_state_;
}

base::TimeDelta TraySessionLengthLimit::GetRemainingSessionTime() const {
  return remaining_session_time_;
}

void TraySessionLengthLimit::Update() {
  SystemTrayDelegate* delegate = Shell::GetInstance()->system_tray_delegate();
  const LimitState previous_limit_state = limit_state_;
  if (!delegate->GetSessionStartTime(&session_start_time_) ||
      !delegate->GetSessionLengthLimit(&limit_)) {
    remaining_session_time_ = base::TimeDelta();
    limit_state_ = LIMIT_NONE;
    timer_.reset();
  } else {
    remaining_session_time_ = std::max(
        limit_ - (base::TimeTicks::Now() - session_start_time_),
        base::TimeDelta());
    limit_state_ = remaining_session_time_.InSeconds() <=
        kExpiringSoonThresholdInSeconds ? LIMIT_EXPIRING_SOON : LIMIT_SET;
    if (!timer_)
      timer_.reset(new base::RepeatingTimer<TraySessionLengthLimit>);
    if (!timer_->IsRunning()) {
      // Start a timer that will update the remaining session time every second.
      timer_->Start(FROM_HERE,
                    base::TimeDelta::FromSeconds(1),
                    this,
                    &TraySessionLengthLimit::Update);
    }
  }

  switch (limit_state_) {
    case LIMIT_NONE:
      message_center::MessageCenter::Get()->RemoveNotification(
          kNotificationId, false /* by_user */);
      break;
    case LIMIT_SET:
      CreateOrUpdateNotification(
          kNotificationId,
          remaining_session_time_,
          previous_limit_state == LIMIT_NONE);
      break;
    case LIMIT_EXPIRING_SOON:
      CreateOrUpdateNotification(
          kNotificationId,
          remaining_session_time_,
          previous_limit_state == LIMIT_NONE ||
          previous_limit_state == LIMIT_SET);
      break;
  }

  // Update the tray view last so that it can check whether the notification
  // view is currently visible or not.
  if (tray_view_)
    tray_view_->Update();
}

bool TraySessionLengthLimit::IsTrayViewVisibleForTest() {
  return tray_view_ && tray_view_->visible();
}

}  // namespace ash
