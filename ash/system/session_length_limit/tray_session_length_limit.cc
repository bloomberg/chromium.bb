// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/session_length_limit/tray_session_length_limit.h"

#include <algorithm>

#include "ash/shelf/shelf_types.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_notification_view.h"
#include "ash/system/tray/tray_views.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

namespace ash {
namespace internal {

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
  gfx::Font font = label->font();
  label->SetFont(font.DeriveFont(0, font.GetStyle() & ~gfx::Font::BOLD));
  return label;
}

string16 IntToTwoDigitString(int value) {
  DCHECK_GE(value, 0);
  DCHECK_LE(value, 99);
  if (value < 10)
    return ASCIIToUTF16("0") + base::IntToString16(value);
  return base::IntToString16(value);
}

string16 FormatRemainingSessionTimeNotification(
    const base::TimeDelta& remaining_session_time) {
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_REMAINING_SESSION_TIME_NOTIFICATION,
      Shell::GetInstance()->system_tray_delegate()->
          FormatTimeDuration(remaining_session_time));
}

}  // namespace

namespace tray {

class RemainingSessionTimeNotificationView : public TrayNotificationView {
 public:
  explicit RemainingSessionTimeNotificationView(TraySessionLengthLimit* owner);
  virtual ~RemainingSessionTimeNotificationView();

  // TrayNotificationView:
  virtual void ChildPreferredSizeChanged(views::View* child) OVERRIDE;

  void Update();

 private:
  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(RemainingSessionTimeNotificationView);
};

class RemainingSessionTimeTrayView : public views::View {
 public:
  RemainingSessionTimeTrayView(const TraySessionLengthLimit* owner,
                               ShelfAlignment shelf_alignment);
  virtual ~RemainingSessionTimeTrayView();

  void UpdateClockLayout(ShelfAlignment shelf_alignment);
  void Update();

 private:
  void SetBorder(ShelfAlignment shelf_alignment);

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

RemainingSessionTimeNotificationView::RemainingSessionTimeNotificationView(
    TraySessionLengthLimit* owner)
    : TrayNotificationView(owner,
                           IDR_AURA_UBER_TRAY_SESSION_LENGTH_LIMIT_TIMER) {
  label_ = new views::Label;
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetMultiLine(true);
  Update();
  InitView(label_);
}

RemainingSessionTimeNotificationView::~RemainingSessionTimeNotificationView() {
}

void RemainingSessionTimeNotificationView::ChildPreferredSizeChanged(
    views::View* child) {
  PreferredSizeChanged();
}

void RemainingSessionTimeNotificationView::Update() {
  label_->SetText(FormatRemainingSessionTimeNotification(
      reinterpret_cast<TraySessionLengthLimit*>(owner())->
          GetRemainingSessionTime()));
}

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
  SetBorder(shelf_alignment);
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
    layout->AddPaddingRow(0, kTrayLabelItemVerticalPaddingVeriticalAlignment);
    layout->StartRow(0, 0);
    layout->AddView(vertical_layout_label_hours_left_);
    layout->AddView(vertical_layout_label_hours_right_);
    layout->StartRow(0, 0);
    layout->AddView(vertical_layout_label_minutes_left_);
    layout->AddView(vertical_layout_label_minutes_right_);
    layout->StartRow(0, 0);
    layout->AddView(vertical_layout_label_seconds_left_);
    layout->AddView(vertical_layout_label_seconds_right_);
    layout->AddPaddingRow(0, kTrayLabelItemVerticalPaddingVeriticalAlignment);
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

  const string16 hours_str = IntToTwoDigitString(hours);
  const string16 minutes_str = IntToTwoDigitString(minutes);
  const string16 seconds_str = IntToTwoDigitString(seconds);
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

void RemainingSessionTimeTrayView::SetBorder(ShelfAlignment shelf_alignment) {
  if (shelf_alignment == SHELF_ALIGNMENT_BOTTOM ||
      shelf_alignment == SHELF_ALIGNMENT_TOP) {
    set_border(views::Border::CreateEmptyBorder(
        0, kTrayLabelItemHorizontalPaddingBottomAlignment,
        0, kTrayLabelItemHorizontalPaddingBottomAlignment));
  } else {
    set_border(NULL);
  }
}

}  // namespace tray

TraySessionLengthLimit::TraySessionLengthLimit(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      tray_view_(NULL),
      notification_view_(NULL),
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

views::View* TraySessionLengthLimit::CreateNotificationView(
    user::LoginStatus status) {
  CHECK(notification_view_ == NULL);
  notification_view_ = new tray::RemainingSessionTimeNotificationView(this);
  return notification_view_;
}

void TraySessionLengthLimit::DestroyTrayView() {
  tray_view_ = NULL;
}

void TraySessionLengthLimit::DestroyNotificationView() {
  notification_view_ = NULL;
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

void TraySessionLengthLimit::ShowAndSpeakNotification() {
  ShowNotificationView();
  Shell::GetInstance()->system_tray_delegate()->MaybeSpeak(UTF16ToUTF8(
      FormatRemainingSessionTimeNotification(remaining_session_time_)));
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

  if (notification_view_)
    notification_view_->Update();

  switch (limit_state_) {
    case LIMIT_NONE:
      HideNotificationView();
      break;
    case LIMIT_SET:
      if (previous_limit_state == LIMIT_NONE)
        ShowAndSpeakNotification();
      break;
    case LIMIT_EXPIRING_SOON:
      if (previous_limit_state == LIMIT_NONE ||
          previous_limit_state == LIMIT_SET) {
        ShowAndSpeakNotification();
      }
      break;
  }

  // Update the tray view last so that it can check whether the notification
  // view is currently visible or not.
  if (tray_view_)
    tray_view_->Update();
}

}  // namespace internal
}  // namespace ash
