// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DATE_TRAY_DATE_H_
#define ASH_SYSTEM_DATE_TRAY_DATE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/system/date/clock_observer.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/macros.h"

namespace views {
class Label;
}

namespace ash {
class DateDefaultView;
#if defined(OS_CHROMEOS)
class SystemClockObserver;
#endif

namespace tray {
class TimeView;
}

class ASH_EXPORT TrayDate : public SystemTrayItem, public ClockObserver {
 public:
  enum ClockLayout {
    HORIZONTAL_CLOCK,
    VERTICAL_CLOCK,
  };

  enum DateAction {
    NONE,
    SET_SYSTEM_TIME,
    SHOW_DATE_SETTINGS,
  };

  explicit TrayDate(SystemTray* system_tray);
  ~TrayDate() override;

  // Returns view for help button if it is exists. Returns NULL otherwise.
  views::View* GetHelpButtonView() const;

  const tray::TimeView* GetTimeTrayForTesting() const;
  const DateDefaultView* GetDefaultViewForTesting() const;
  views::View* CreateDefaultViewForTesting(user::LoginStatus status);

 private:
  // Overridden from SystemTrayItem.
  views::View* CreateTrayView(user::LoginStatus status) override;
  views::View* CreateDefaultView(user::LoginStatus status) override;
  views::View* CreateDetailedView(user::LoginStatus status) override;
  void DestroyTrayView() override;
  void DestroyDefaultView() override;
  void DestroyDetailedView() override;
  void UpdateAfterLoginStatusChange(user::LoginStatus status) override;
  void UpdateAfterShelfAlignmentChange(wm::ShelfAlignment alignment) override;

  // Overridden from ClockObserver.
  void OnDateFormatChanged() override;
  void OnSystemClockTimeUpdated() override;
  void OnSystemClockCanSetTimeChanged(bool can_set_time) override;
  void Refresh() override;

  void SetupLabelForTimeTray(views::Label* label);

  tray::TimeView* time_tray_;
  DateDefaultView* default_view_;
  user::LoginStatus login_status_;

#if defined(OS_CHROMEOS)
  std::unique_ptr<SystemClockObserver> system_clock_observer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(TrayDate);
};

}  // namespace ash

#endif  // ASH_SYSTEM_DATE_TRAY_DATE_H_
