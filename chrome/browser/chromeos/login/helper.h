// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains helper functions used by Chromium OS login.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_HELPER_H_
#pragma once

#include "views/controls/button/native_button.h"
#include "third_party/skia/include/core/SkColor.h"

class GURL;

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace views {
class Label;
class Painter;
class Textfield;
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

// Corrects font size for NativeButton control.
void CorrectLabelFontSize(views::Label* label);

// Corrects font size for NativeButton control.
void CorrectNativeButtonFontSize(views::NativeButton* button);

// Corrects font size for Textfield control.
void CorrectTextfieldFontSize(views::Textfield* textfield);

// Returns URL used for account recovery.
GURL GetAccountRecoveryHelpUrl();

// Define the constants in |login| namespace to avoid potential
// conflict with other chromeos components.
namespace login {

// Command tag for buttons on the lock screen.
enum Command {
  SIGN_OUT,
};

// Gap between edge and image view, and image view and controls.
const int kBorderSize = 6;

// The size of user image.
const int kUserImageSize = 256;

// Background color of the login controls.
const SkColor kBackgroundColor = SK_ColorWHITE;

// Text color on the login controls.
const SkColor kTextColor = SK_ColorWHITE;

// Default link color on login/OOBE controls.
const SkColor kLinkColor = 0xFF0066CC;

// Default size of the OOBE screen. Includes 10px shadow from each side.
// See rounded_rect_painter.cc for border definitions.
const int kWizardScreenWidth = 800;
const int kWizardScreenHeight = 450;

const int kScreenCornerRadius = 10;
const int kUserCornerRadius = 5;

// Username label height in different states.
const int kSelectedLabelHeight = 25;
const int kUnselectedLabelHeight = 20;

class WideButton : public views::NativeButton {
 public:
  WideButton(views::ButtonListener* listener, const std::wstring& text)
      : NativeButton(listener, text) {
    CorrectNativeButtonFontSize(this);
  }

  ~WideButton() {}

 private:
  virtual gfx::Size GetPreferredSize();

  DISALLOW_COPY_AND_ASSIGN(WideButton);
};

}  // namespace login

// Font size correction in points for login/oobe textfields/buttons/title.
const int kFontSizeCorrectionDelta = 2;

// New pod sizes.
const int kNewUserPodFullWidth = 372;
const int kNewUserPodFullHeight = 372;
const int kNewUserPodSmallWidth = 360;
const int kNewUserPodSmallHeight = 322;

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_HELPER_H_
