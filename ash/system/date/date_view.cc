// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/date/date_view.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "grit/ash_strings.h"
#include "third_party/icu/source/i18n/unicode/datefmt.h"
#include "third_party/icu/source/i18n/unicode/dtptngen.h"
#include "third_party/icu/source/i18n/unicode/smpdtfmt.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
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
const int kVerticalClockMinutesTopOffset = -4;

base::string16 FormatDate(const base::Time& time) {
  icu::UnicodeString date_string;
  std::unique_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createDateInstance(icu::DateFormat::kMedium));
  formatter->format(static_cast<UDate>(time.ToDoubleT() * 1000), date_string);
  return base::string16(date_string.getBuffer(),
                  static_cast<size_t>(date_string.length()));
}

base::string16 FormatDayOfWeek(const base::Time& time) {
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::DateTimePatternGenerator> generator(
      icu::DateTimePatternGenerator::createInstance(status));
  DCHECK(U_SUCCESS(status));
  const char kBasePattern[] = "EEE";
  icu::UnicodeString generated_pattern =
      generator->getBestPattern(icu::UnicodeString(kBasePattern), status);
  DCHECK(U_SUCCESS(status));
  icu::SimpleDateFormat simple_formatter(generated_pattern, status);
  DCHECK(U_SUCCESS(status));
  icu::UnicodeString date_string;
  simple_formatter.format(
      static_cast<UDate>(time.ToDoubleT() * 1000), date_string, status);
  DCHECK(U_SUCCESS(status));
  return base::string16(
      date_string.getBuffer(), static_cast<size_t>(date_string.length()));
}

views::Label* CreateLabel() {
  views::Label* label = new views::Label;
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetBackgroundColor(SkColorSetARGB(0, 255, 255, 255));
  return label;
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

BaseDateTimeView::BaseDateTimeView() {
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
  timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(seconds_left),
      this, &BaseDateTimeView::UpdateText);
}

void BaseDateTimeView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void BaseDateTimeView::OnLocaleChanged() {
  UpdateText();
}

DateView::DateView()
    : hour_type_(ash::Shell::GetInstance()->system_tray_delegate()->
                 GetHourClockType()),
      action_(TrayDate::NONE) {
  SetLayoutManager(
      new views::BoxLayout(
          views::BoxLayout::kVertical, 0, 0, 0));
  date_label_ = CreateLabel();
  date_label_->SetEnabledColor(kHeaderTextColorNormal);
  UpdateTextInternal(base::Time::Now());
  AddChildView(date_label_);
}

DateView::~DateView() {
}

void DateView::SetAction(TrayDate::DateAction action) {
  if (action == action_)
    return;
  if (IsMouseHovered()) {
    date_label_->SetEnabledColor(
        action == TrayDate::NONE ? kHeaderTextColorNormal :
                                   kHeaderTextColorHover);
    SchedulePaint();
  }
  action_ = action;
  SetFocusBehavior(action_ != TrayDate::NONE ? FocusBehavior::ALWAYS
                                             : FocusBehavior::NEVER);
}

void DateView::UpdateTimeFormat() {
  hour_type_ =
      ash::Shell::GetInstance()->system_tray_delegate()->GetHourClockType();
  UpdateText();
}

base::HourClockType DateView::GetHourTypeForTesting() const {
  return hour_type_;
}

void DateView::SetActive(bool active) {
  date_label_->SetEnabledColor(active ? kHeaderTextColorHover
                                      : kHeaderTextColorNormal);
  SchedulePaint();
}

void DateView::UpdateTextInternal(const base::Time& now) {
  SetAccessibleName(
      base::TimeFormatFriendlyDate(now) +
      base::ASCIIToUTF16(", ") +
      base::TimeFormatTimeOfDayWithHourClockType(
          now, hour_type_, base::kKeepAmPm));
  date_label_->SetText(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_DATE, FormatDayOfWeek(now), FormatDate(now)));
}

bool DateView::PerformAction(const ui::Event& event) {
  if (action_ == TrayDate::NONE)
    return false;
  if (action_ == TrayDate::SHOW_DATE_SETTINGS)
    ash::Shell::GetInstance()->system_tray_delegate()->ShowDateSettings();
  else if (action_ == TrayDate::SET_SYSTEM_TIME)
    ash::Shell::GetInstance()->system_tray_delegate()->ShowSetTimeDialog();
  return true;
}

void DateView::OnMouseEntered(const ui::MouseEvent& event) {
  if (action_ == TrayDate::NONE)
    return;
  SetActive(true);
}

void DateView::OnMouseExited(const ui::MouseEvent& event) {
  if (action_ == TrayDate::NONE)
    return;
  SetActive(false);
}

void DateView::OnGestureEvent(ui::GestureEvent* event) {
  if (switches::IsTouchFeedbackEnabled()) {
    if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
      SetActive(true);
    } else if (event->type() == ui::ET_GESTURE_TAP_CANCEL ||
               event->type() == ui::ET_GESTURE_END) {
      SetActive(false);
    }
  }
  BaseDateTimeView::OnGestureEvent(event);
}

///////////////////////////////////////////////////////////////////////////////

TimeView::TimeView(TrayDate::ClockLayout clock_layout)
    : hour_type_(ash::Shell::GetInstance()->system_tray_delegate()->
                 GetHourClockType()) {
  SetupLabels();
  UpdateTextInternal(base::Time::Now());
  UpdateClockLayout(clock_layout);
}

TimeView::~TimeView() {
}

void TimeView::UpdateTimeFormat() {
  hour_type_ =
      ash::Shell::GetInstance()->system_tray_delegate()->GetHourClockType();
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

  base::string16 current_time = base::TimeFormatTimeOfDayWithHourClockType(
      now, hour_type_, base::kDropAmPm);
  horizontal_label_->SetText(current_time);
  horizontal_label_->SetTooltipText(base::TimeFormatFriendlyDate(now));

  // Calculate vertical clock layout labels.
  size_t colon_pos = current_time.find(base::ASCIIToUTF16(":"));
  base::string16 hour = current_time.substr(0, colon_pos);
  base::string16 minute = current_time.substr(colon_pos + 1);

  // Sometimes pad single-digit hours with a zero for aesthetic reasons.
  if (hour.length() == 1 &&
      hour_type_ == base::k24HourClock &&
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

void TimeView::UpdateClockLayout(TrayDate::ClockLayout clock_layout) {
  SetBorderFromLayout(clock_layout);
  if (clock_layout == TrayDate::HORIZONTAL_CLOCK) {
    RemoveChildView(vertical_label_hours_.get());
    RemoveChildView(vertical_label_minutes_.get());
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
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
    layout->AddPaddingRow(0, kTrayLabelItemVerticalPaddingVerticalAlignment);
    layout->StartRow(0, kColumnId);
    layout->AddView(vertical_label_hours_.get());
    layout->StartRow(0, kColumnId);
    layout->AddView(vertical_label_minutes_.get());
    layout->AddPaddingRow(0, kTrayLabelItemVerticalPaddingVerticalAlignment);
  }
  Layout();
}

void TimeView::SetBorderFromLayout(TrayDate::ClockLayout clock_layout) {
  if (clock_layout == TrayDate::HORIZONTAL_CLOCK)
    SetBorder(views::Border::CreateEmptyBorder(
        0,
        kTrayLabelItemHorizontalPaddingBottomAlignment,
        0,
        kTrayLabelItemHorizontalPaddingBottomAlignment));
  else
    SetBorder(views::Border::NullBorder());
}

void TimeView::SetupLabels() {
  horizontal_label_.reset(CreateLabel());
  SetupLabel(horizontal_label_.get());
  vertical_label_hours_.reset(CreateLabel());
  SetupLabel(vertical_label_hours_.get());
  vertical_label_minutes_.reset(CreateLabel());
  SetupLabel(vertical_label_minutes_.get());
  vertical_label_minutes_->SetEnabledColor(kVerticalClockMinuteColor);
  // Pull the minutes up closer to the hours by using a negative top border.
  vertical_label_minutes_->SetBorder(views::Border::CreateEmptyBorder(
      kVerticalClockMinutesTopOffset, 0, 0, 0));
}

void TimeView::SetupLabel(views::Label* label) {
  label->set_owned_by_client();
  SetupLabelForTray(label);
  label->SetElideBehavior(gfx::NO_ELIDE);
}

}  // namespace tray
}  // namespace ash
