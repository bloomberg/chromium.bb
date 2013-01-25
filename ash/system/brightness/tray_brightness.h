// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BRIGHTNESS_TRAY_BRIGHTNESS_H_
#define ASH_SYSTEM_BRIGHTNESS_TRAY_BRIGHTNESS_H_

#include "ash/system/brightness/brightness_observer.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"

namespace ash {
namespace internal {

namespace tray {
class BrightnessView;
}

class TrayBrightness : public SystemTrayItem,
                       public BrightnessObserver {
 public:
  explicit TrayBrightness(SystemTray* system_tray);
  virtual ~TrayBrightness();

 private:
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
  virtual bool ShouldShowLauncher() const OVERRIDE;

  // Overridden from BrightnessObserver.
  virtual void OnBrightnessChanged(double percent,
                                   bool user_initiated) OVERRIDE;

  base::WeakPtrFactory<TrayBrightness> weak_ptr_factory_;

  tray::BrightnessView* brightness_view_;

  // Was |brightness_view_| created for CreateDefaultView() rather than
  // CreateDetailedView()?  Used to avoid resetting |brightness_view_|
  // inappropriately in DestroyDefaultView() or DestroyDetailedView().
  bool is_default_view_;

  // Brightness level in the range [0.0, 100.0] that we've heard about most
  // recently.
  double current_percent_;

  // Has |current_percent_| been initialized?
  bool got_current_percent_;

  DISALLOW_COPY_AND_ASSIGN(TrayBrightness);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_BRIGHTNESS_TRAY_BRIGHTNESS_H_
