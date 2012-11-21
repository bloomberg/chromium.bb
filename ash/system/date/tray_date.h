// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_DATE_TRAY_DATE_H_
#define ASH_SYSTEM_DATE_TRAY_DATE_H_

#include "ash/system/date/clock_observer.h"
#include "ash/system/tray/system_tray_item.h"

namespace views {
class Label;
}

namespace ash {
namespace internal {

namespace tray {
class TimeView;
}

class TrayDate : public SystemTrayItem,
                 public ClockObserver {
 public:
  enum ClockLayout {
   HORIZONTAL_CLOCK,
   VERTICAL_CLOCK,
  };
  explicit TrayDate(SystemTray* system_tray);
  virtual ~TrayDate();

 private:
  // Overridden from SystemTrayItem.
  virtual views::View* CreateTrayView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDetailedView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyTrayView() OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyDetailedView() OVERRIDE;
  virtual void UpdateAfterLoginStatusChange(user::LoginStatus status) OVERRIDE;
  virtual void UpdateAfterShelfAlignmentChange(
      ShelfAlignment alignment) OVERRIDE;

  // Overridden from ClockObserver.
  virtual void OnDateFormatChanged() OVERRIDE;
  virtual void Refresh() OVERRIDE;

  void SetupLabelForTimeTray(views::Label* label);

  tray::TimeView* time_tray_;

  DISALLOW_COPY_AND_ASSIGN(TrayDate);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_DATE_TRAY_DATE_H_
