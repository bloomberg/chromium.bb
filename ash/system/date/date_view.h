// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DATE_DATE_VIEW_H_
#define ASH_SYSTEM_DATE_DATE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/system/date/tray_date.h"
#include "ash/system/tray/actionable_view.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer/timer.h"
#include "ui/views/view.h"

namespace views {
class Label;
}

namespace ash {
namespace tray {

// Abstract base class containing common updating and layout code for the
// DateView popup and the TimeView tray icon. Exported for tests.
class ASH_EXPORT BaseDateTimeView : public ActionableView {
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
class ASH_EXPORT DateView : public BaseDateTimeView {
 public:
  DateView();
  virtual ~DateView();

  // Sets the action the view should take. An actionable date view gives visual
  // feedback on hover, can be focused by keyboard, and clicking/pressing space
  // or enter on the view executes the action.
  void SetAction(TrayDate::DateAction action);

  // Updates the format of the displayed time.
  void UpdateTimeFormat();

  base::HourClockType GetHourTypeForTesting() const;

 private:
  // Overridden from BaseDateTimeView.
  virtual void UpdateTextInternal(const base::Time& now) OVERRIDE;

  // Overridden from ActionableView.
  virtual bool PerformAction(const ui::Event& event) OVERRIDE;

  // Overridden from views::View.
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;

  views::Label* date_label_;

  // Time format (12/24hr) used for accessibility string.
  base::HourClockType hour_type_;

  TrayDate::DateAction action_;

  DISALLOW_COPY_AND_ASSIGN(DateView);
};

// Tray view used to display the current time.
// Exported for tests.
class ASH_EXPORT TimeView : public BaseDateTimeView {
 public:
  explicit TimeView(TrayDate::ClockLayout clock_layout);
  virtual ~TimeView();

  // Updates the format of the displayed time.
  void UpdateTimeFormat();

  // Updates clock layout.
  void UpdateClockLayout(TrayDate::ClockLayout clock_layout);

  base::HourClockType GetHourTypeForTesting() const;

 private:
  friend class TimeViewTest;

  // Overridden from BaseDateTimeView.
  virtual void UpdateTextInternal(const base::Time& now) OVERRIDE;

  // Overridden from ActionableView.
  virtual bool PerformAction(const ui::Event& event) OVERRIDE;

  // Overridden from views::View.
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;

  void SetBorderFromLayout(TrayDate::ClockLayout clock_layout);
  void SetupLabels();
  void SetupLabel(views::Label* label);

  // Label text used for the normal horizontal shelf.
  scoped_ptr<views::Label> horizontal_label_;

  // The time label is split into two lines for the vertical shelf.
  scoped_ptr<views::Label> vertical_label_hours_;
  scoped_ptr<views::Label> vertical_label_minutes_;

  base::HourClockType hour_type_;

  DISALLOW_COPY_AND_ASSIGN(TimeView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_DATE_DATE_VIEW_H_
