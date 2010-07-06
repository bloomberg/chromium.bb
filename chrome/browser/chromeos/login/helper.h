// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains helper functions used by Chromium OS login.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_HELPER_H_

#include "third_party/skia/include/core/SkColor.h"

namespace views {
class Painter;
class Throbber;
}  // namespace views

namespace chromeos {

// Creates default smoothed throbber for time consuming operations on login.
views::Throbber* CreateDefaultSmoothedThrobber();

// Creates default throbber.
views::Throbber* CreateDefaultThrobber();

// Creates painter for login background.
views::Painter* CreateBackgroundPainter();

// Returns bounds of the screen to use for login wizard.
// The rect is centered within the default monitor and sized accordingly if
// |size| is not empty. Otherwise the whole monitor is occupied.
gfx::Rect CalculateScreenBounds(const gfx::Size& size);

// Define the constants in |login| namespace to avoid potential
// conflict with other chromeos components.
namespace login {

// Command tag for buttons on the lock screen.
enum Command {
  UNLOCK,
  SIGN_OUT,
};

// Gap between edge and image view, and image view and controls.
const int kBorderSize = 4;

// The size of user image.
const int kUserImageSize = 256;

// Background color of the login controls.
const SkColor kBackgroundColor = SK_ColorWHITE;

// Text color on the login controls.
const SkColor kTextColor = SK_ColorWHITE;

}  // namespace login

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_HELPER_H_
