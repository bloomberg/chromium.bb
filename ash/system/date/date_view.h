// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DATE_DATE_VIEW_H_
#define ASH_SYSTEM_DATE_DATE_VIEW_H_
#pragma once

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

// This view is used for both the TrayDate tray icon and the TrayPower popup.
class DateView : public ActionableView {
 public:
  enum TimeType {
    TIME,
    DATE
  };

  explicit DateView(TimeType type);
  virtual ~DateView();
  void UpdateTimeFormat();
  views::Label* label() const { return label_; }

  // Sets whether the view is actionable. An actionable date view gives visual
  // feedback on hover, can be focused by keyboard, and clicking/pressing space
  // or enter on the view shows date-related settings.
  void SetActionable(bool actionable);

  void UpdateText();

 private:
  // Overridden from ActionableView.
  virtual bool PerformAction(const views::Event& event) OVERRIDE;

  // Overridden from views::View.
  virtual void OnLocaleChanged() OVERRIDE;

  base::OneShotTimer<DateView> timer_;
  base::HourClockType hour_type_;
  TimeType type_;
  bool actionable_;
  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(DateView);
};

}  // namespace tray
}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_DATE_DATE_VIEW_H_
