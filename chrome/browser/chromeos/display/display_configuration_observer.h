// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DISPLAY_DISPLAY_CONFIGURATION_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_DISPLAY_DISPLAY_CONFIGURATION_OBSERVER_H_

#include "ash/display/window_tree_host_manager.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace chromeos {

// DisplayConfigurationObserver observes and saves
// the change of display configurations.
class DisplayConfigurationObserver
    : public ash::WindowTreeHostManager::Observer {
 public:
  DisplayConfigurationObserver();
  ~DisplayConfigurationObserver() override;

 protected:
  // ash::WindowTreeHostManager::Observer overrides:
  void OnDisplaysInitialized() override;
  void OnDisplayConfigurationChanged() override;

  DISALLOW_COPY_AND_ASSIGN(DisplayConfigurationObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DISPLAY_DISPLAY_CONFIGURATION_OBSERVER_H_
