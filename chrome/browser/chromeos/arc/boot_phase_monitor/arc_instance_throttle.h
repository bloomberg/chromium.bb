// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_BOOT_PHASE_MONITOR_ARC_INSTANCE_THROTTLE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_BOOT_PHASE_MONITOR_ARC_INSTANCE_THROTTLE_H_

#include "ash/common/wm_activation_observer.h"
#include "base/macros.h"

namespace arc {

// A class that watches window activations and prioritizes the ARC instance when
// one of ARC windows is activated. The class also unprioritizes the instance
// when non-ARC window such as Chrome is activated.
class ArcInstanceThrottle : public ash::WmActivationObserver {
 public:
  ArcInstanceThrottle();
  ~ArcInstanceThrottle() override;

  // ash::WmActivationObserver overrides:
  void OnWindowActivated(ash::WmWindow* gained_active,
                         ash::WmWindow* lost_active) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcInstanceThrottle);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_BOOT_PHASE_MONITOR_ARC_INSTANCE_THROTTLE_H_
