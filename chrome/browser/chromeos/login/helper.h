// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains helper functions used by Chromium OS login.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_HELPER_H_

#include "third_party/skia/include/core/SkColor.h"

namespace views {
class Throbber;
}  // namespace views

namespace chromeos {

// Creates default smoothed throbber for time consuming operations on login.
views::Throbber* CreateDefaultSmoothedThrobber();

// Creates default throbber.
views::Throbber* CreateDefaultThrobber();

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
const int kUserImageSize = 260;

// Background color of the login controls.
const SkColor kBackgroundColor = SK_ColorWHITE;

// Text color on the login controls.
const SkColor kTextColor = SK_ColorWHITE;

}  // namespace login

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_HELPER_H_
