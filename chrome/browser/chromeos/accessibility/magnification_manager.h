// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_MAGNIFICATION_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_MAGNIFICATION_MANAGER_H_

#include "chrome/browser/chromeos/accessibility/accessibility_util.h"

namespace chromeos {

// MagnificationManager controls the full screen magnifier from chrome-browser
// side (not ash side).
//
// MagnificationManager does:
//   - Watch logged-in. Changes the behavior between the login screen and user
//     desktop.
//   - Watch change of the pref. When the pref changes, the setting of the
//     magnifier will interlock with it.

class MagnificationManager {
 public:
  // Creates an instance of MagnificationManager. This should be called once,
  // because only one instance should exist at the same time.
  static MagnificationManager* CreateInstance();
  // Returns the existing instance. If there is no instance, returns NULL.
  static MagnificationManager* GetInstance();

  virtual ~MagnificationManager() {}

  // Returns the current type of the screen magnifier.
  virtual ash::MagnifierType GetMagnifierType() = 0;
  // Changes the type of the screen magnifier.
  virtual void SetMagnifier(ash::MagnifierType type) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_MAGNIFICATION_MANAGER_H_
