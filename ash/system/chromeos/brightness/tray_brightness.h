// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_BRIGHTNESS_TRAY_BRIGHTNESS_H_
#define ASH_SYSTEM_CHROMEOS_BRIGHTNESS_TRAY_BRIGHTNESS_H_

#include "ash/system/tray/system_tray_item.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/power_manager_client.h"

namespace ash {
namespace tray {
class BrightnessView;
}

class ASH_EXPORT TrayBrightness
    : public SystemTrayItem,
      public chromeos::PowerManagerClient::Observer {
 public:
  explicit TrayBrightness(SystemTray* system_tray);
  virtual ~TrayBrightness();

 private:
  friend class TrayBrightnessTest;

  // Sends a request to get the current screen brightness so |current_percent_|
  // can be initialized.
  void GetInitialBrightness();

  // Updates |current_percent_| with the initial brightness requested by
  // GetInitialBrightness(), if we haven't seen the brightness already in the
  // meantime.
  void HandleInitialBrightness(double percent);

  // Overridden from SystemTrayItem.
  virtual views::View* CreateTrayView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDetailedView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyTrayView() OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyDetailedView() OVERRIDE;
  virtual void UpdateAfterLoginStatusChange(user::LoginStatus status) OVERRIDE;
  virtual bool ShouldHideArrow() const OVERRIDE;
  virtual bool ShouldShowShelf() const OVERRIDE;

  // Overriden from PowerManagerClient::Observer.
  virtual void BrightnessChanged(int level, bool user_initiated) OVERRIDE;

  void HandleBrightnessChanged(double percent, bool user_initiated);

  tray::BrightnessView* brightness_view_;

  // Brightness level in the range [0.0, 100.0] that we've heard about most
  // recently.
  double current_percent_;

  // Has |current_percent_| been initialized?
  bool got_current_percent_;

  base::WeakPtrFactory<TrayBrightness> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TrayBrightness);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_BRIGHTNESS_TRAY_BRIGHTNESS_H_
