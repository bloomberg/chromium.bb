// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/date/date_view.h"

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/system_tray_controller.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "base/i18n/rtl.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "third_party/icu/source/i18n/unicode/datefmt.h"
#include "third_party/icu/source/i18n/unicode/dtptngen.h"
#include "third_party/icu/source/i18n/unicode/smpdtfmt.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace tray {
namespace {

// Amount of slop to add into the timer to make sure we're into the next minute
// when the timer goes off.
const int kTimerSlopSeconds = 1;

// Text color of the vertical clock minutes.
const SkColor kVerticalClockMinuteColor = SkColorSetRGB(0xBA, 0xBA, 0xBA);

// Padding between the left edge of the shelf and the left edge of the vertical
// clock.
const int kVerticalClockLeftPadding = 9;

// Offset used to bring the minutes line closer to the hours line in the
// vertical clock.
const int kVerticalClockMinutesTopOffset = -2;

// Leading padding used to draw the tray background to the left of the clock
// when the shelf is vertically aligned.
const int kClockLeadingPadding = 8;

base::string16 FormatDate(const base::Time& time) {
  // Use 'short' month format (e.g., "Oct") followed by non-padded day of
  // month (e.g., "2", "10").
  return base::TimeFormatWithPattern(time, "LLLd");
}

base::string16 FormatDayOfWeek(const base::Time& time) {
  // Use 'short' day of week format (e.g., "Wed").
  return base::TimeFormatWithPattern(time, "EEE");
}

}  // namespace

BaseDateTimeView::~BaseDateTimeView() {
  timer_.Stop();
}

void BaseDateTimeView::UpdateText() {
  base::Time now = base::Time::Now();
  UpdateTextInternal(now);
  SchedulePaint();
  SetTimer(now);
}

void BaseDateTimeView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  ActionableView::GetAccessibleNodeData(node_data);
  node_data->role = ui::AX_ROLE_TIME;
}

BaseDateTimeView::BaseDateTimeView(SystemTrayItem* owner)
    : ActionableView(owner, TrayPopupInkDropStyle::INSET_BOUNDS),
      hour_type_(Shell::Get()->system_tray_controller()->hour_clock_type()) {
  SetTimer(base::Time::Now());
  SetFocusBehavior(FocusBehavior::NEVER);
}

void BaseDateTimeView::SetTimer(const base::Time& now) {
  // Try to set the timer to go off at the next change of the minute. We don't
  // want to have the timer go off more than necessary since that will cause
  // the CPU to wake up and consume power.
  base::Time::Exploded exploded;
  now.LocalExplode(&exploded);

  // Often this will be called at minute boundaries, and we'll actually want
  // 60 seconds from now.
  int seconds_left = 60 - exploded.second;
  if (seconds_left == 0)
    seconds_left = 60;

  // Make sure that the timer fires on the next minute. Without this, if it is
  // called just a teeny bit early, then it will skip the next minute.
  seconds_left += kTimerSlopSeconds;

  timer_.Stop();
  timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(seconds_left), this,
               &BaseDateTimeView::UpdateText);
}

void BaseDateTimeView::UpdateTextInternal(const base::Time& now) {
  SetAccessibleName(base::TimeFormatTimeOfDayWithHourClockType(
                        now, hour_type_, base::kKeepAmPm) +
                    base::ASCIIToUTF16(", ") +
                    base::TimeFormatFriendlyDate(now));

  NotifyAccessibilityEvent(ui::AX_EVENT_TEXT_CHANGED, true);
}

void BaseDateTimeView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

///////////////////////////////////////////////////////////////////////////////

DateView::DateView(SystemTrayItem* owner)
    : BaseDateTimeView(owner), action_(DateAction::NONE) {
  views::BoxLayout* box_layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal,
                           gfx::Insets(0, kTrayPopupLabelHorizontalPadding), 0);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  SetLayoutManager(box_layout);
  date_label_ = TrayPopupUtils::CreateDefaultLabel();
  UpdateTextInternal(base::Time::Now());
  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::SYSTEM_INFO);
  style.SetupLabel(date_label_);
  AddChildView(date_label_);
}

DateView::~DateView() {}

void DateView::SetAction(DateAction action) {
  if (action == action_)
    return;
  action_ = action;
  SetFocusBehavior(action_ != DateAction::NONE ? FocusBehavior::ALWAYS
                                               : FocusBehavior::NEVER);

  // Disable |this| when not clickable so that the ripple is not shown.
  SetEnabled(action_ != DateAction::NONE);
  if (action_ != DateAction::NONE)
    SetInkDropMode(views::InkDropHostView::InkDropMode::ON);
}

void DateView::UpdateTimeFormat() {
  hour_type_ = Shell::Get()->system_tray_controller()->hour_clock_type();
  UpdateText();
}

base::HourClockType DateView::GetHourTypeForTesting() const {
  return hour_type_;
}

void DateView::UpdateTextInternal(const base::Time& now) {
  BaseDateTimeView::UpdateTextInternal(now);
  date_label_->SetText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_DATE, FormatDayOfWeek(now), FormatDate(now)));
}

bool DateView::PerformAction(const ui::Event& event) {
  switch (action_) {
    case DateAction::SHOW_DATE_SETTINGS:
      Shell::Get()->system_tray_controller()->ShowDateSettings();
      break;
    case DateAction::SET_SYSTEM_TIME:
      Shell::Get()->system_tray_controller()->ShowSetTimeDialog();
      break;
    case DateAction::NONE:
      return false;
  }
  CloseSystemBubble();
  return true;
}

///////////////////////////////////////////////////////////////////////////////

TimeView::TimeView(ClockLayout clock_layout) : BaseDateTimeView(nullptr) {
  SetupLabels();
  UpdateTextInternal(base::Time::Now());
  UpdateClockLayout(clock_layout);
}

TimeView::~TimeView() {}

void TimeView::UpdateTimeFormat() {
  hour_type_ = Shell::Get()->system_tray_controller()->hour_clock_type();
  UpdateText();
}

base::HourClockType TimeView::GetHourTypeForTesting() const {
  return hour_type_;
}

void TimeView::UpdateTextInternal(const base::Time& now) {
  // Just in case |now| is null, do NOT update time; otherwise, it will
  // crash icu code by calling into base::TimeFormatTimeOfDayWithHourClockType,
  // see details in crbug.com/147570.
  if (now.is_null()) {
    LOG(ERROR) << "Received null value from base::Time |now| in argument";
    return;
  }

  BaseDateTimeView::UpdateTextInternal(now);
  base::string16 current_time = base::TimeFormatTimeOfDayWithHourClockType(
      now, hour_type_, base::kDropAmPm);
  horizontal_label_->SetText(current_time);
  horizontal_label_->SetTooltipText(base::TimeFormatFriendlyDate(now));

  // Calculate vertical clock layout labels.
  size_t colon_pos = current_time.find(base::ASCIIToUTF16(":"));
  base::string16 hour = current_time.substr(0, colon_pos);
  base::string16 minute = current_time.substr(colon_pos + 1);

  // Sometimes pad single-digit hours with a zero for aesthetic reasons.
  if (hour.length() == 1 && hour_type_ == base::k24HourClock &&
      !base::i18n::IsRTL())
    hour = base::ASCIIToUTF16("0") + hour;

  vertical_label_hours_->SetText(hour);
  vertical_label_minutes_->SetText(minute);
  Layout();
}

bool TimeView::PerformAction(const ui::Event& event) {
  return false;
}

bool TimeView::OnMousePressed(const ui::MouseEvent& event) {
  // Let the event fall through.
  return false;
}

void TimeView::OnGestureEvent(ui::GestureEvent* event) {
  // Skip gesture handling happening in Button so that the container views
  // receive and handle them properly.
  // TODO(mohsen): Refactor TimeView/DateView classes so that they are not
  // ActionableView anymore. Create an ActionableView as a container for when
  // needed.
}

void TimeView::UpdateClockLayout(ClockLayout clock_layout) {
  SetBorderFromLayout(clock_layout);
  if (clock_layout == ClockLayout::HORIZONTAL_CLOCK) {
    RemoveChildView(vertical_label_hours_.get());
    RemoveChildView(vertical_label_minutes_.get());
    SetLayoutManager(new views::FillLayout());
    AddChildView(horizontal_label_.get());
  } else {
    RemoveChildView(horizontal_label_.get());
    views::GridLayout* layout = new views::GridLayout(this);
    SetLayoutManager(layout);
    const int kColumnId = 0;
    views::ColumnSet* columns = layout->AddColumnSet(kColumnId);
    columns->AddPaddingColumn(0, kVerticalClockLeftPadding);
    columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                       0, views::GridLayout::USE_PREF, 0, 0);
    layout->AddPaddingRow(0, kClockLeadingPadding);
    layout->StartRow(0, kColumnId);
    layout->AddView(vertical_label_hours_.get());
    layout->StartRow(0, kColumnId);
    layout->AddView(vertical_label_minutes_.get());
    layout->AddPaddingRow(
        0, kTrayImageItemPadding + kVerticalClockMinutesTopOffset);
  }
  Layout();
}

void TimeView::SetBorderFromLayout(ClockLayout clock_layout) {
  if (clock_layout == ClockLayout::HORIZONTAL_CLOCK)
    SetBorder(views::CreateEmptyBorder(gfx::Insets(0, kTrayImageItemPadding)));
  else
    SetBorder(views::NullBorder());
}

void TimeView::SetupLabels() {
  horizontal_label_.reset(new views::Label());
  SetupLabel(horizontal_label_.get());
  vertical_label_hours_.reset(new views::Label());
  SetupLabel(vertical_label_hours_.get());
  vertical_label_minutes_.reset(new views::Label());
  SetupLabel(vertical_label_minutes_.get());
  // TODO(estade): this should use the NativeTheme's secondary text color. See
  // crbug.com/687791
  vertical_label_minutes_->SetEnabledColor(kVerticalClockMinuteColor);
  // Pull the minutes up closer to the hours by using a negative top border.
  vertical_label_minutes_->SetBorder(
      views::CreateEmptyBorder(kVerticalClockMinutesTopOffset, 0, 0, 0));
}

void TimeView::SetupLabel(views::Label* label) {
  label->set_owned_by_client();
  SetupLabelForTray(label);
  label->SetElideBehavior(gfx::NO_ELIDE);
}

}  // namespace tray
}  // namespace ash
