// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/date/date_view.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "base/i18n/time_formatting.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"
#include "unicode/datefmt.h"
#include "unicode/dtptngen.h"
#include "unicode/smpdtfmt.h"

namespace ash {
namespace internal {
namespace tray {

namespace {

// Amount of slop to add into the timer to make sure we're into the next minute
// when the timer goes off.
const int kTimerSlopSeconds = 1;

// Top number text color of vertical clock.
const SkColor kVerticalClockHourColor = SkColorSetRGB(0xBA, 0xBA, 0xBA);

string16 FormatDate(const base::Time& time) {
  icu::UnicodeString date_string;
  scoped_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createDateInstance(icu::DateFormat::kMedium));
  formatter->format(static_cast<UDate>(time.ToDoubleT() * 1000), date_string);
  return string16(date_string.getBuffer(),
                  static_cast<size_t>(date_string.length()));
}

string16 FormatDayOfWeek(const base::Time& time) {
  UErrorCode status = U_ZERO_ERROR;
  scoped_ptr<icu::DateTimePatternGenerator> generator(
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
  return string16(
      date_string.getBuffer(), static_cast<size_t>(date_string.length()));
}

views::Label* CreateLabel() {
  views::Label* label = new views::Label;
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
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

DateView::DateView() : actionable_(false) {
  SetLayoutManager(
      new views::BoxLayout(
          views::BoxLayout::kVertical, 0, 0, 0));
  date_label_ = CreateLabel();
  date_label_->SetEnabledColor(kHeaderTextColorNormal);
  UpdateTextInternal(base::Time::Now());
  AddChildView(date_label_);
  set_focusable(actionable_);
}

DateView::~DateView() {
}

void DateView::SetActionable(bool actionable) {
  actionable_ = actionable;
  set_focusable(actionable_);
}

void DateView::UpdateTextInternal(const base::Time& now) {
  date_label_->SetText(
      l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_DATE, FormatDayOfWeek(now), FormatDate(now)));
}

bool DateView::PerformAction(const ui::Event& event) {
  if (!actionable_)
    return false;

  ash::Shell::GetInstance()->tray_delegate()->ShowDateSettings();
  return true;
}

void DateView::OnMouseEntered(const ui::MouseEvent& event) {
  if (!actionable_)
    return;
  date_label_->SetEnabledColor(kHeaderTextColorHover);
  SchedulePaint();
}

void DateView::OnMouseExited(const ui::MouseEvent& event) {
  if (!actionable_)
    return;
  date_label_->SetEnabledColor(kHeaderTextColorNormal);
  SchedulePaint();
}

TimeView::TimeView(TrayDate::ClockLayout clock_layout)
    : hour_type_(
          ash::Shell::GetInstance()->tray_delegate()->GetHourClockType()) {
  SetupLabels();
  UpdateTextInternal(base::Time::Now());
  UpdateClockLayout(clock_layout);
  set_focusable(false);
}

TimeView::~TimeView() {
}

void TimeView::UpdateTimeFormat() {
  hour_type_ = ash::Shell::GetInstance()->tray_delegate()->GetHourClockType();
  UpdateText();
}

void TimeView::UpdateTextInternal(const base::Time& now) {
  // Just in case |now| is null, do NOT update time; otherwise, it will
  // crash icu code by calling into base::TimeFormatTimeOfDayWithHourClockType,
  // see details in crbug.com/147570.
  if (now.is_null()) {
    LOG(ERROR) << "Received null value from base::Time |now| in argument";
    return;
  }

  string16 current_time = base::TimeFormatTimeOfDayWithHourClockType(
      now, hour_type_, base::kDropAmPm);
  label_->SetText(current_time);
  label_->SetTooltipText(base::TimeFormatFriendlyDate(now));

  // Calculate vertical clock layout labels.
  size_t colon_pos = current_time.find(ASCIIToUTF16(":"));
  string16 hour = current_time.substr(0, colon_pos);
  string16 minute = current_time.substr(colon_pos + 1);
  label_hour_left_->SetText(hour.substr(0, 1));
  label_hour_right_->SetText(hour.length() == 2 ?
      hour.substr(1,1) : ASCIIToUTF16(":"));
  label_minute_left_->SetText(minute.substr(0, 1));
  label_minute_right_->SetText(minute.substr(1, 1));

  Layout();
}

bool TimeView::PerformAction(const ui::Event& event) {
  return false;
}

bool TimeView::OnMousePressed(const ui::MouseEvent& event) {
  // Let the event fall through.
  return false;
}

void TimeView::UpdateClockLayout(TrayDate::ClockLayout clock_layout){
  SetBorder(clock_layout);
  if (clock_layout == TrayDate::HORIZONTAL_CLOCK) {
    RemoveChildView(label_hour_left_.get());
    RemoveChildView(label_hour_right_.get());
    RemoveChildView(label_minute_left_.get());
    RemoveChildView(label_minute_right_.get());
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
    AddChildView(label_.get());
  } else {
    RemoveChildView(label_.get());
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
    layout->AddView(label_hour_left_.get());
    layout->AddView(label_hour_right_.get());
    layout->StartRow(0, 0);
    layout->AddView(label_minute_left_.get());
    layout->AddView(label_minute_right_.get());
    layout->AddPaddingRow(0, kTrayLabelItemVerticalPaddingVeriticalAlignment);
  }
  Layout();
}

void TimeView::SetBorder(TrayDate::ClockLayout clock_layout) {
  if (clock_layout == TrayDate::HORIZONTAL_CLOCK)
    set_border(views::Border::CreateEmptyBorder(
        0, kTrayLabelItemHorizontalPaddingBottomAlignment,
        0, kTrayLabelItemHorizontalPaddingBottomAlignment));
  else
    set_border(NULL);
}

void TimeView::SetupLabels() {
  label_.reset(CreateLabel());
  SetupLabel(label_.get());
  label_hour_left_.reset(CreateLabel());
  SetupLabel(label_hour_left_.get());
  label_hour_right_.reset(CreateLabel());
  SetupLabel(label_hour_right_.get());
  label_minute_left_.reset(CreateLabel());
  SetupLabel(label_minute_left_.get());
  label_minute_right_.reset(CreateLabel());
  SetupLabel(label_minute_right_.get());
  label_hour_left_->SetEnabledColor(kVerticalClockHourColor);
  label_hour_right_->SetEnabledColor(kVerticalClockHourColor);
}

void TimeView::SetupLabel(views::Label* label) {
  label->set_owned_by_client();
  SetupLabelForTray(label);
  gfx::Font font = label->font();
  label->SetFont(font.DeriveFont(0, font.GetStyle() & ~gfx::Font::BOLD));
}

}  // namespace tray
}  // namespace internal
}  // namespace ash
