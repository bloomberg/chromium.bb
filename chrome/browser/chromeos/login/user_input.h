// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_INPUT_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_INPUT_H_

#include "ui/gfx/rect.h"

namespace chromeos {

// Interface that is used to manage the state of the user input controls.
class UserInput {
 public:
  virtual ~UserInput() {}

  // Enables/Disables the input controls.
  virtual void EnableInputControls(bool enabled) = 0;

  // Clears and focuses the controls.
  virtual void ClearAndFocusControls() = 0;

  // Clears and focuses the password field.
  virtual void ClearAndFocusPassword() = 0;

  // Returns bounds of the main input field in the screen coordinates (e.g.
  // these bounds could be used to choose positions for the error bubble).
  virtual gfx::Rect GetMainInputScreenBounds() const = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_INPUT_H_
