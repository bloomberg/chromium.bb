// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BRIGHTNESS_TRAY_BRIGHTNESS_H_
#define ASH_SYSTEM_BRIGHTNESS_TRAY_BRIGHTNESS_H_
#pragma once

#include "ash/system/brightness/brightness_observer.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/compiler_specific.h"

namespace ash {
namespace internal {

namespace tray {
class BrightnessView;
}

class TrayBrightness : public SystemTrayItem,
                       public BrightnessObserver {
 public:
  TrayBrightness();
  virtual ~TrayBrightness();

 private:
  // Overridden from SystemTrayItem.
  virtual views::View* CreateTrayView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDetailedView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyTrayView() OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyDetailedView() OVERRIDE;

  // Overridden from BrightnessObserver.
  virtual void OnBrightnessChanged(float percent,
                                   bool user_initiated) OVERRIDE;

  scoped_ptr<tray::BrightnessView> brightness_view_;

  DISALLOW_COPY_AND_ASSIGN(TrayBrightness);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_BRIGHTNESS_TRAY_BRIGHTNESS_H_
