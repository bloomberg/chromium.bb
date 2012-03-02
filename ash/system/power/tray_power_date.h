// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_TRAY_POWER_DATE_H_
#define ASH_SYSTEM_POWER_TRAY_POWER_DATE_H_
#pragma once

#include "ash/system/power/power_status_controller.h"
#include "ash/system/tray/system_tray_item.h"

namespace ash {
namespace internal {

namespace tray {
class DateView;
class PowerPopupView;
class PowerTrayView;
}

class TrayPowerDate : public SystemTrayItem,
                      public PowerStatusController {
 public:
  TrayPowerDate();
  virtual ~TrayPowerDate();

 private:
  // Overridden from SystemTrayItem.
  virtual views::View* CreateTrayView() OVERRIDE;
  virtual views::View* CreateDefaultView() OVERRIDE;
  virtual views::View* CreateDetailedView() OVERRIDE;
  virtual void DestroyTrayView() OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyDetailedView() OVERRIDE;

  // Overridden from PowerStatusController.
  virtual void OnPowerStatusChanged(const PowerSupplyStatus& status) OVERRIDE;

  scoped_ptr<tray::DateView> date_;
  scoped_ptr<tray::DateView> date_tray_;

  scoped_ptr<tray::PowerPopupView> power_;
  scoped_ptr<tray::PowerTrayView> power_tray_;

  DISALLOW_COPY_AND_ASSIGN(TrayPowerDate);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_POWER_TRAY_POWER_DATE_H_
