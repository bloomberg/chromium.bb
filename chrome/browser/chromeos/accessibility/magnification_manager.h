// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_MAGNIFICATION_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_MAGNIFICATION_MANAGER_H_

#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"

class Profile;

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
  // Creates an instance of MagnificationManager. This should be called once.
  static void Initialize();

  // Deletes the existing instance of MagnificationManager.
  static void Shutdown();

  // Returns the existing instance. If there is no instance, returns NULL.
  static MagnificationManager* Get();

  // Returns if the screen magnifier is enabled.
  virtual bool IsMagnifierEnabled() const = 0;

  // Enables the screen magnifier.
  virtual void SetMagnifierEnabled(bool enabled) = 0;

  // Saves the magnifier scale to the pref.
  virtual void SaveScreenMagnifierScale(double scale) = 0;

  // Loads the magnifier scale from the pref.
  virtual double GetSavedScreenMagnifierScale() const = 0;

  virtual void SetProfileForTest(Profile* profile) = 0;

 protected:
  virtual ~MagnificationManager() {}
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_MAGNIFICATION_MANAGER_H_
