// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_SYSTEM_TRAY_DELEGATE_MUS_H_
#define ASH_MUS_SYSTEM_TRAY_DELEGATE_MUS_H_

#include "ash/common/system/tray/default_system_tray_delegate.h"
#include "ash/public/interfaces/system_tray.mojom.h"
#include "base/i18n/time_formatting.h"
#include "base/macros.h"

namespace ash {

// Handles the settings displayed in the system tray menu. For mus most settings
// are obtained from chrome browser via mojo IPC. For the classic ash version
// see SystemTrayDelegateChromeOS.
//
// TODO: Support all methods in SystemTrayDelegate. http://crbug.com/647412.
class SystemTrayDelegateMus : public DefaultSystemTrayDelegate,
                              public mojom::SystemTray {
 public:
  SystemTrayDelegateMus();
  ~SystemTrayDelegateMus() override;

  static SystemTrayDelegateMus* Get();

 private:
  // SystemTrayDelegate:
  base::HourClockType GetHourClockType() const override;

  // mojom::SystemTray:
  void SetUse24HourClock(bool use_24_hour) override;

  // 12 or 24 hour display.
  base::HourClockType hour_clock_type_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayDelegateMus);
};

}  // namespace ash

#endif  // ASH_MUS_SYSTEM_TRAY_DELEGATE_MUS_H_
