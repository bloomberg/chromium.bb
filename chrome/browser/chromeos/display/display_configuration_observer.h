// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DISPLAY_DISPLAY_CONFIGURATION_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_DISPLAY_DISPLAY_CONFIGURATION_OBSERVER_H_

#include "ash/display/window_tree_host_manager.h"
#include "ash/wm/tablet_mode/tablet_mode_observer.h"
#include "base/compiler_specific.h"
#include "base/macros.h"

namespace chromeos {

// DisplayConfigurationObserver observes and saves
// the change of display configurations.
class DisplayConfigurationObserver
    : public ash::WindowTreeHostManager::Observer,
      public ash::TabletModeObserver {
 public:
  DisplayConfigurationObserver();
  ~DisplayConfigurationObserver() override;

 protected:
  // ash::WindowTreeHostManager::Observer:
  void OnDisplaysInitialized() override;
  void OnDisplayConfigurationChanged() override;

  // ash::TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

  bool save_preference_ = true;

  // True if the device was in mirror mode before siwtching to
  // tablet mode.
  bool was_in_mirror_mode_ = false;

  DISALLOW_COPY_AND_ASSIGN(DisplayConfigurationObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DISPLAY_DISPLAY_CONFIGURATION_OBSERVER_H_
