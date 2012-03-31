// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/date/date_view.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
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

string16 FormatNicely(const base::Time& time) {
  icu::UnicodeString date_string;

  scoped_ptr<icu::DateFormat> formatter(
      icu::DateFormat::createDateInstance(icu::DateFormat::kFull));
  icu::FieldPosition position;
  position.setField(UDAT_DAY_OF_WEEK_FIELD);
  formatter->format(static_cast<UDate>(time.ToDoubleT() * 1000),
                    date_string,
                    position);
  icu::UnicodeString day = date_string.retainBetween(position.getBeginIndex(),
                                                     position.getEndIndex());

  date_string.remove();
  formatter.reset(
      icu::DateFormat::createDateInstance(icu::DateFormat::kMedium));
  formatter->format(static_cast<UDate>(time.ToDoubleT() * 1000), date_string);

  date_string += "\n";
  date_string += day;

  return string16(date_string.getBuffer(),
                  static_cast<size_t>(date_string.length()));
}

}

DateView::DateView(TimeType type)
    : hour_type_(ash::Shell::GetInstance()->tray_delegate()->
          GetHourClockType()),
      type_(type),
      actionable_(false) {
  // TODO(flackr): Investigate respecting the view's border in FillLayout.
  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, 0));
  label_ = new views::Label;
  label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label_->SetBackgroundColor(SkColorSetARGB(0, 255, 255, 255));
  UpdateText();
  AddChildView(label_);
  set_focusable(actionable_);
}

DateView::~DateView() {
  timer_.Stop();
}

void DateView::UpdateTimeFormat() {
  hour_type_ = ash::Shell::GetInstance()->tray_delegate()->GetHourClockType();
  UpdateText();
}

void DateView::SetActionable(bool actionable) {
  actionable_ = actionable;
  set_focusable(actionable_);
}

void DateView::UpdateText() {
  base::Time now = base::Time::Now();
  gfx::Size old_size = label_->GetPreferredSize();
  if (type_ == DATE) {
    label_->SetText(FormatNicely(now));
  } else {
    label_->SetText(base::TimeFormatTimeOfDayWithHourClockType(
        now, hour_type_, base::kDropAmPm));
  }
  if (label_->GetPreferredSize() != old_size && GetWidget()) {
    // Forcing the widget to the new size is sufficient. The positing is taken
    // care of by the layout manager (ShelfLayoutManager).
    GetWidget()->SetSize(GetWidget()->GetContentsView()->GetPreferredSize());
  }

  label_->SetTooltipText(base::TimeFormatFriendlyDate(now));
  SchedulePaint();

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
      &DateView::UpdateText);
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
  gfx::Font font = label_->font();
  label_->SetFont(font.DeriveFont(0,
        font.GetStyle() | gfx::Font::UNDERLINED));
  SchedulePaint();
}

void DateView::OnMouseExited(const views::MouseEvent& event) {
  if (!actionable_)
    return;
  gfx::Font font = label_->font();
  label_->SetFont(font.DeriveFont(0,
        font.GetStyle() & ~gfx::Font::UNDERLINED));
  SchedulePaint();
}

void DateView::OnLocaleChanged() {
  UpdateText();
}

}  // namespace tray
}  // namespace internal
}  // namespace ash
