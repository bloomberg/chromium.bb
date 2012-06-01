// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DATE_DATE_VIEW_H_
#define ASH_SYSTEM_DATE_DATE_VIEW_H_
#pragma once

#include "ash/system/date/tray_date.h"
#include "ash/system/tray/tray_views.h"
#include "base/i18n/time_formatting.h"
#include "base/timer.h"
#include "ui/views/view.h"

namespace views {
class Label;
}

namespace ash {
namespace internal {
namespace tray {

// Abstract base class containing common updating and layout code for the
// DateView popup and the TimeView tray icon.
class BaseDateTimeView : public ActionableView {
 public:
  virtual ~BaseDateTimeView();

  // Updates the displayed text for the current time and calls SetTimer().
  void UpdateText();

 protected:
  BaseDateTimeView();

 private:
  // Starts |timer_| to schedule the next update.
  void SetTimer(const base::Time& now);

  // Updates labels to display the current time.
  virtual void UpdateTextInternal(const base::Time& now) = 0;

  // Overridden from views::View.
  virtual void ChildPreferredSizeChanged(views::View* child) OVERRIDE;
  virtual void OnLocaleChanged() OVERRIDE;

  // Invokes UpdateText() when the displayed time should change.
  base::OneShotTimer<BaseDateTimeView> timer_;

  DISALLOW_COPY_AND_ASSIGN(BaseDateTimeView);
};

// Popup view used to display the date and day of week.
class DateView : public BaseDateTimeView {
 public:
  DateView();
  virtual ~DateView();

  // Sets whether the view is actionable. An actionable date view gives visual
  // feedback on hover, can be focused by keyboard, and clicking/pressing space
  // or enter on the view shows date-related settings.
  void SetActionable(bool actionable);

 private:
  // Overridden from BaseDateTimeView.
  virtual void UpdateTextInternal(const base::Time& now) OVERRIDE;

  // Overridden from ActionableView.
  virtual bool PerformAction(const views::Event& event) OVERRIDE;

  // Overridden from views::View.
  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;

  views::Label* date_label_;
  views::Label* day_of_week_label_;

  bool actionable_;

  DISALLOW_COPY_AND_ASSIGN(DateView);
};

// Tray view used to display the current time.
class TimeView : public BaseDateTimeView {
 public:
  TimeView();
  virtual ~TimeView();

  views::Label* label() const { return label_.get(); }
  views::Label* label_hour() const { return label_hour_.get(); }
  views::Label* label_minute() const { return label_minute_.get(); }

  // Updates the format of the displayed time.
  void UpdateTimeFormat();

  // Updates clock layout.
  void UpdateClockLayout(TrayDate::ClockLayout clock_layout);

 private:
  // Overridden from BaseDateTimeView.
  virtual void UpdateTextInternal(const base::Time& now) OVERRIDE;

  // Overridden from ActionableView.
  virtual bool PerformAction(const views::Event& event) OVERRIDE;

  // Overridden from views::View.
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;

  void SetBorder(TrayDate::ClockLayout clock_layout);

  scoped_ptr<views::Label> label_;
  scoped_ptr<views::Label> label_hour_;
  scoped_ptr<views::Label> label_minute_;

  base::HourClockType hour_type_;

  DISALLOW_COPY_AND_ASSIGN(TimeView);
};

}  // namespace tray
}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_DATE_DATE_VIEW_H_
