// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DATE_DATE_VIEW_H_
#define ASH_SYSTEM_DATE_DATE_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/system/tray/actionable_view.h"
#include "base/i18n/time_formatting.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "ui/views/view.h"

namespace base {
class Time;
}

namespace views {
class Label;
}

namespace ash {
namespace tray {

// Abstract base class containing common updating and layout code for the
// DateView popup and the TimeView tray icon. Exported for tests.
class ASH_EXPORT BaseDateTimeView : public ActionableView {
 public:
  ~BaseDateTimeView() override;

  // Updates the displayed text for the current time and calls SetTimer().
  void UpdateText();

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 protected:
  explicit BaseDateTimeView(SystemTrayItem* owner);

  // Updates labels to display the current time.
  virtual void UpdateTextInternal(const base::Time& now);

  // Time format (12/24hr) used for accessibility string.
  base::HourClockType hour_type_;

 private:
  // Starts |timer_| to schedule the next update.
  void SetTimer(const base::Time& now);

  // views::View:
  void ChildPreferredSizeChanged(views::View* child) override;

  // Invokes UpdateText() when the displayed time should change.
  base::OneShotTimer timer_;

  DISALLOW_COPY_AND_ASSIGN(BaseDateTimeView);
};

// Popup view used to display the date and day of week.
class ASH_EXPORT DateView : public BaseDateTimeView {
 public:
  enum class DateAction {
    NONE,
    SET_SYSTEM_TIME,
    SHOW_DATE_SETTINGS,
  };

  explicit DateView(SystemTrayItem* owner);
  ~DateView() override;

  // Sets the action the view should take. An actionable date view gives visual
  // feedback on hover, can be focused by keyboard, and clicking/pressing space
  // or enter on the view executes the action.
  void SetAction(DateAction action);

  // Updates the format of the displayed time.
  void UpdateTimeFormat();

  base::HourClockType GetHourTypeForTesting() const;

 private:
  // Sets active rendering state and updates the color of |date_label_|.
  void SetActive(bool active);

  // BaseDateTimeView:
  void UpdateTextInternal(const base::Time& now) override;

  // ActionableView:
  bool PerformAction(const ui::Event& event) override;

  views::Label* date_label_;

  DateAction action_;

  DISALLOW_COPY_AND_ASSIGN(DateView);
};

// Tray view used to display the current time.
// Exported for tests.
class ASH_EXPORT TimeView : public BaseDateTimeView {
 public:
  enum class ClockLayout {
    HORIZONTAL_CLOCK,
    VERTICAL_CLOCK,
  };

  explicit TimeView(ClockLayout clock_layout);
  ~TimeView() override;

  // Updates the format of the displayed time.
  void UpdateTimeFormat();

  // Updates clock layout.
  void UpdateClockLayout(ClockLayout clock_layout);

  base::HourClockType GetHourTypeForTesting() const;

 private:
  friend class TimeViewTest;

  // BaseDateTimeView:
  void UpdateTextInternal(const base::Time& now) override;

  // ActionableView:
  bool PerformAction(const ui::Event& event) override;

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  void SetBorderFromLayout(ClockLayout clock_layout);
  void SetupLabels();
  void SetupLabel(views::Label* label);

  // Label text used for the normal horizontal shelf.
  std::unique_ptr<views::Label> horizontal_label_;

  // The time label is split into two lines for the vertical shelf.
  std::unique_ptr<views::Label> vertical_label_hours_;
  std::unique_ptr<views::Label> vertical_label_minutes_;

  DISALLOW_COPY_AND_ASSIGN(TimeView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_DATE_DATE_VIEW_H_
