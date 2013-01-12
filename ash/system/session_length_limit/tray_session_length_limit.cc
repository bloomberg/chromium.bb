// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/session_length_limit/tray_session_length_limit.h"

#include <cmath>

#include "ash/shelf_types.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
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

namespace tray {

namespace {

// Warning threshold for the remaining sessiont time.
const int kRemainingTimeWarningThresholdInSeconds = 5 * 60;  // 5 minutes.
// Color in which the remaining session time is normally shown.
const SkColor kRemainingTimeColor = SK_ColorWHITE;
// Color in which the remaining session time is shown when it falls below the
// warning threshold.
const SkColor kRemainingTimeWarningColor = SK_ColorRED;

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

}  // namespace

class RemainingSessionTimeTrayView : public views::View {
 public:
  RemainingSessionTimeTrayView(const base::Time& session_start_time,
                               const base::TimeDelta& limit,
                               ShelfAlignment shelf_alignment);
  virtual ~RemainingSessionTimeTrayView();

  void SetSessionStartTime(const base::Time& session_start_time);
  void SetSessionLengthLimit(const base::TimeDelta& limit);

  void UpdateClockLayout(ShelfAlignment shelf_alignment);

 private:
  void SetBorder(ShelfAlignment shelf_alignment);

  // Update the label text only.
  void UpdateText();
  // Update the timer state, label text and visibility.
  void UpdateState();

  views::Label* horizontal_layout_label_;
  views::Label* vertical_layout_label_hours_left_;
  views::Label* vertical_layout_label_hours_right_;
  views::Label* vertical_layout_label_minutes_left_;
  views::Label* vertical_layout_label_minutes_right_;
  views::Label* vertical_layout_label_seconds_left_;
  views::Label* vertical_layout_label_seconds_right_;

  base::Time session_start_time_;
  base::TimeDelta limit_;
  base::RepeatingTimer<RemainingSessionTimeTrayView> timer_;

  DISALLOW_COPY_AND_ASSIGN(RemainingSessionTimeTrayView);
};

RemainingSessionTimeTrayView::RemainingSessionTimeTrayView(
    const base::Time& session_start_time,
    const base::TimeDelta& limit,
    ShelfAlignment shelf_alignment)
    : horizontal_layout_label_(NULL),
      vertical_layout_label_hours_left_(NULL),
      vertical_layout_label_hours_right_(NULL),
      vertical_layout_label_minutes_left_(NULL),
      vertical_layout_label_minutes_right_(NULL),
      vertical_layout_label_seconds_left_(NULL),
      vertical_layout_label_seconds_right_(NULL),
      session_start_time_(session_start_time),
      limit_(limit) {
  UpdateClockLayout(shelf_alignment);
  UpdateState();
}

RemainingSessionTimeTrayView::~RemainingSessionTimeTrayView() {
}

void RemainingSessionTimeTrayView::SetSessionStartTime(
    const base::Time& session_start_time) {
  session_start_time_ = session_start_time;
  UpdateState();
}

void RemainingSessionTimeTrayView::SetSessionLengthLimit(
    const base::TimeDelta& limit) {
  limit_ = limit;
  UpdateState();
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
  UpdateText();
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

void RemainingSessionTimeTrayView::UpdateText() {
  if (!visible())
    return;

  // Calculate the remaining session time, clamping so that it never falls below
  // zero or exceeds 99 hours.
  int seconds = floor(
      (limit_ - (base::Time::Now() - session_start_time_)).InSecondsF() + .5);
  seconds = std::min(std::max(seconds, 0), 99 * 60 * 60);
  const SkColor color = seconds < kRemainingTimeWarningThresholdInSeconds ?
      kRemainingTimeWarningColor : kRemainingTimeColor;
  int minutes = seconds / 60;
  seconds %= 60;
  const int hours = minutes / 60;
  minutes %= 60;

  const string16 hours_str = IntToTwoDigitString(hours);
  const string16 minutes_str = IntToTwoDigitString(minutes);
  const string16 seconds_str = IntToTwoDigitString(seconds);

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
}

void RemainingSessionTimeTrayView::UpdateState() {
  const bool show = session_start_time_ != base::Time() &&
                    limit_ != base::TimeDelta();
  SetVisible(show);
  UpdateText();
  if (show && !timer_.IsRunning()) {
    // Set timer to update the text once per second.
    timer_.Start(FROM_HERE,
                 base::TimeDelta::FromSeconds(1),
                 this,
                 &RemainingSessionTimeTrayView::UpdateText);
  } else if (!show && timer_.IsRunning()) {
    timer_.Stop();
  }
}

}  // namespace tray

TraySessionLengthLimit::TraySessionLengthLimit(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      tray_view_(NULL) {
  Shell::GetInstance()->system_tray_notifier()->
      AddSessionLengthLimitObserver(this);
}

TraySessionLengthLimit::~TraySessionLengthLimit() {
  Shell::GetInstance()->system_tray_notifier()->
      RemoveSessionLengthLimitObserver(this);
}

views::View* TraySessionLengthLimit::CreateTrayView(user::LoginStatus status) {
  CHECK(tray_view_ == NULL);
  ash::SystemTrayDelegate* delegate =
      ash::Shell::GetInstance()->system_tray_delegate();
  tray_view_ = new tray::RemainingSessionTimeTrayView(
      delegate->GetSessionStartTime(),
      delegate->GetSessionLengthLimit(),
      system_tray()->shelf_alignment());
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

void TraySessionLengthLimit::OnSessionStartTimeChanged(
    const base::Time& start_time) {
  tray_view_->SetSessionStartTime(start_time);
}

void TraySessionLengthLimit::OnSessionLengthLimitChanged(
    const base::TimeDelta& limit) {
  tray_view_->SetSessionLengthLimit(limit);
}

}  // namespace internal
}  // namespace ash
