// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/date/date_view.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "base/time.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"
#include "unicode/datefmt.h"

namespace ash {
namespace internal {
namespace tray {

namespace {

// Amount of slop to add into the timer to make sure we're into the next minute
// when the timer goes off.
const int kTimerSlopSeconds = 1;

string16 FormatDate(const base::Time& time) {
  icu::UnicodeString date_string;
  scoped_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createDateInstance(icu::DateFormat::kMedium));
  formatter->format(static_cast<UDate>(time.ToDoubleT() * 1000), date_string);
  return string16(date_string.getBuffer(),
                  static_cast<size_t>(date_string.length()));
}

string16 FormatDayOfWeek(const base::Time& time) {
  scoped_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createDateInstance(icu::DateFormat::kFull));
  icu::UnicodeString date_string;
  icu::FieldPosition position;
  position.setField(UDAT_DAY_OF_WEEK_FIELD);
  formatter->format(
      static_cast<UDate>(time.ToDoubleT() * 1000), date_string, position);
  icu::UnicodeString day = date_string.retainBetween(position.getBeginIndex(),
                                                     position.getEndIndex());
  return string16(day.getBuffer(), static_cast<size_t>(day.length()));
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
  views::View::PreferredSizeChanged();
  if (GetWidget())
    GetWidget()->SetSize(GetWidget()->GetContentsView()->GetPreferredSize());
}

void BaseDateTimeView::OnLocaleChanged() {
  UpdateText();
}

DateView::DateView() : actionable_(false) {
  SetLayoutManager(
      new views::BoxLayout(
          views::BoxLayout::kVertical, 0, 0, kTrayPopupTextSpacingVertical));
  date_label_ = CreateLabel();
  date_label_->SetFont(date_label_->font().DeriveFont(0, gfx::Font::BOLD));
  day_of_week_label_ = CreateLabel();
  date_label_->SetEnabledColor(kHeaderTextColorNormal);
  day_of_week_label_->SetEnabledColor(kHeaderTextColorNormal);
  UpdateTextInternal(base::Time::Now());
  AddChildView(date_label_);
  AddChildView(day_of_week_label_);
  set_focusable(actionable_);
}

DateView::~DateView() {
}

void DateView::SetActionable(bool actionable) {
  actionable_ = actionable;
  set_focusable(actionable_);
}

void DateView::UpdateTextInternal(const base::Time& now) {
  date_label_->SetText(FormatDate(now));
  day_of_week_label_->SetText(FormatDayOfWeek(now));
}

bool DateView::PerformAction(const views::Event& event) {
  if (!actionable_)
    return false;

  ash::Shell::GetInstance()->tray_delegate()->ShowDateSettings();
  return true;
}

void DateView::OnMouseEntered(const views::MouseEvent& event) {
  if (!actionable_)
    return;
  date_label_->SetEnabledColor(kHeaderTextColorHover);
  day_of_week_label_->SetEnabledColor(kHeaderTextColorHover);
  SchedulePaint();
}

void DateView::OnMouseExited(const views::MouseEvent& event) {
  if (!actionable_)
    return;
  date_label_->SetEnabledColor(kHeaderTextColorNormal);
  day_of_week_label_->SetEnabledColor(kHeaderTextColorNormal);
  SchedulePaint();
}

TimeView::TimeView()
    : hour_type_(
          ash::Shell::GetInstance()->tray_delegate()->GetHourClockType()) {
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0));
  label_ = CreateLabel();
  UpdateTextInternal(base::Time::Now());
  AddChildView(label_);
  set_focusable(true);
}

TimeView::~TimeView() {
}

void TimeView::UpdateTimeFormat() {
  hour_type_ = ash::Shell::GetInstance()->tray_delegate()->GetHourClockType();
  UpdateText();
}

void TimeView::UpdateTextInternal(const base::Time& now) {
  label_->SetText(
      base::TimeFormatTimeOfDayWithHourClockType(
          now, hour_type_, base::kDropAmPm));
  label_->SetTooltipText(base::TimeFormatFriendlyDate(now));
}

bool TimeView::PerformAction(const views::Event& event) {
  return false;
}


}  // namespace tray
}  // namespace internal
}  // namespace ash
