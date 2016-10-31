// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_DATE_TRAY_SYSTEM_INFO_H_
#define ASH_COMMON_SYSTEM_DATE_TRAY_SYSTEM_INFO_H_

#if defined(OS_CHROMEOS)
#include <memory>
#endif  // defined(OS_CHROMEOS)

#include "ash/ash_export.h"
#include "ash/common/login_status.h"
#include "ash/common/system/date/clock_observer.h"
#include "ash/common/system/tray/system_tray_item.h"
#include "base/macros.h"

namespace views {
class Label;
}

namespace ash {
class SystemInfoDefaultView;
#if defined(OS_CHROMEOS)
class SystemClockObserver;
#endif

namespace tray {
class TimeView;
}

// The bottom row of the system menu. The default view shows the current date
// and power status. The tray view shows the current time.
class ASH_EXPORT TraySystemInfo : public SystemTrayItem, public ClockObserver {
 public:
  explicit TraySystemInfo(SystemTray* system_tray);
  ~TraySystemInfo() override;

 private:
  // SystemTrayItem:
  views::View* CreateTrayView(LoginStatus status) override;
  views::View* CreateDefaultView(LoginStatus status) override;
  void DestroyTrayView() override;
  void DestroyDefaultView() override;
  void UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) override;

  // ClockObserver:
  void OnDateFormatChanged() override;
  void OnSystemClockTimeUpdated() override;
  void OnSystemClockCanSetTimeChanged(bool can_set_time) override;
  void Refresh() override;

  void SetupLabelForTimeTray(views::Label* label);
  void UpdateTimeFormat();

  tray::TimeView* tray_view_;
  SystemInfoDefaultView* default_view_;
  LoginStatus login_status_;

#if defined(OS_CHROMEOS)
  std::unique_ptr<SystemClockObserver> system_clock_observer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(TraySystemInfo);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_DATE_TRAY_SYSTEM_INFO_H_
