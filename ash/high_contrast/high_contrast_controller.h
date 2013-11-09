// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HIGH_CONTRAST_HIGH_CONTRAST_CONTROLLER_H_
#define ASH_HIGH_CONTRAST_HIGH_CONTRAST_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"

namespace aura {
class RootWindow;
class Window;
}

namespace ash {

class ASH_EXPORT HighContrastController {
 public:
  HighContrastController();

  ~HighContrastController() {}

  // Set high contrast mode and update all available displays.
  void SetEnabled(bool enabled);

  // Update high contrast mode on the just added display.
  void OnRootWindowAdded(aura::Window* root_window);

 private:
  // Update high contrast mode on the passed display.
  void UpdateDisplay(aura::Window* root_window);

  // Indicates if the high contrast mode is enabled or disabled.
  bool enabled_;

  DISALLOW_COPY_AND_ASSIGN(HighContrastController);
};

}  // namespace ash

#endif  // ASH_HIGH_CONTRAST_HIGH_CONTRAST_CONTROLLER_H_
