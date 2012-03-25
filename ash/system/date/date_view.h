// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DATE_DATE_VIEW_H_
#define ASH_SYSTEM_DATE_DATE_VIEW_H_
#pragma once

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
class DateView : public views::View {
 public:
  enum TimeType {
    TIME,
    DATE
  };

  explicit DateView(TimeType type);
  virtual ~DateView();
  void UpdateTimeFormat();
  views::Label* label() const { return label_; }

  void set_actionable(bool actionable) { actionable_ = actionable; }

  void UpdateText();

 private:
  // Overridden from views::View.
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE;
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
