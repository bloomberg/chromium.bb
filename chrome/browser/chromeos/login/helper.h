// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains helper functions used by Chromium OS login.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_HELPER_H_
#pragma once

#include "base/string16.h"
#include "third_party/skia/include/core/SkColor.h"
#include "views/controls/button/native_button.h"
#include "views/view.h"
#include "views/widget/widget_gtk.h"

class GURL;

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace views {
class Label;
class MenuButton;
class Painter;
class SmoothedThrobber;
class Textfield;
class Throbber;
class Widget;
}  // namespace views

namespace chromeos {

class NetworkLibrary;

// View that provides interface for start/stop throbber above the view.
class ThrobberHostView : public views::View {
 public:
  ThrobberHostView();
  virtual ~ThrobberHostView();

  // Creates throbber above the view in the right side with the default
  // margin. Also places throbber in the middle of the vertical host size.
  // Override |CalculateThrobberBounds| method to change the throbber placing.
  virtual void StartThrobber();

  // Stops the throbber.
  virtual void StopThrobber();

 protected:
  // Calculates the bounds of the throbber relatively to the host view.
  // Default position is vertically centered right side of the host view.
  virtual gfx::Rect CalculateThrobberBounds(views::Throbber* throbber);

  void set_host_view(views::View* host_view) {
    host_view_ = host_view;
  }

 private:
  // View to show the throbber above (default is |this|).
  views::View* host_view_;

  // Popup that contains the throbber.
  views::Widget* throbber_widget_;

  DISALLOW_COPY_AND_ASSIGN(ThrobberHostView);
};

// Creates default smoothed throbber for time consuming operations on login.
views::SmoothedThrobber* CreateDefaultSmoothedThrobber();

// Creates default throbber.
views::Throbber* CreateDefaultThrobber();

// Creates painter for login background.
views::Painter* CreateBackgroundPainter();

// Returns bounds of the screen to use for login wizard.
// The rect is centered within the default monitor and sized accordingly if
// |size| is not empty. Otherwise the whole monitor is occupied.
gfx::Rect CalculateScreenBounds(const gfx::Size& size);

// Corrects font size for Label control.
void CorrectLabelFontSize(views::Label* label);

// Corrects font size for MenuButton control.
void CorrectMenuButtonFontSize(views::MenuButton* button);

// Corrects font size for NativeButton control.
void CorrectNativeButtonFontSize(views::NativeButton* button);

// Corrects font size for Textfield control.
void CorrectTextfieldFontSize(views::Textfield* textfield);

// Returns URL used for account recovery.
GURL GetAccountRecoveryHelpUrl();

// Returns name of the currently connected network.
// If there are no connected networks, returns name of the network
// that is in the "connecting" state. Otherwise empty string is returned.
// If there are multiple connected networks, network priority:
// Ethernet > WiFi > Cellular. Same for connecting network.
string16 GetCurrentNetworkName(NetworkLibrary* network_library);

// Define the constants in |login| namespace to avoid potential
// conflict with other chromeos components.
namespace login {

// Command tag for buttons on the lock screen.
enum Command {
  SIGN_OUT,
};

// Gap between edge and image view, and image view and controls.
const int kBorderSize = 10;

// The size of user image.
const int kUserImageSize = 256;

// Background color of the login controls.
const SkColor kBackgroundColor = SK_ColorWHITE;

// Text color on the login controls.
const SkColor kTextColor = SK_ColorWHITE;

// Default link color on login/OOBE controls.
const SkColor kLinkColor = 0xFF0066CC;

// Right margin for placing throbber above the view.
const int kThrobberRightMargin = 10;

// Default size of the OOBE screen. Includes 10px shadow from each side.
// See rounded_rect_painter.cc for border definitions.
const int kWizardScreenWidth = 800;
const int kWizardScreenHeight = 450;

const int kScreenCornerRadius = 10;
const int kUserCornerRadius = 6;

// Username label height in different states.
const int kSelectedLabelHeight = 25;
const int kUnselectedLabelHeight = 20;

// Minimal width for the button.
const int kButtonMinWidth = 90;

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

// Font size correction in pixels for login/oobe controls.
#if defined(CROS_FONTS_USING_BCI)
const int kFontSizeCorrectionDelta = 1;
const int kNetworkSelectionLabelFontDelta = 1;
const int kSelectedUsernameFontDelta = 1;
const int kUnselectedUsernameFontDelta = 1;
const int kWelcomeTitleFontDelta = 8;
const int kLoginTitleFontDelta = 3;
#else
const int kFontSizeCorrectionDelta = 2;
const int kNetworkSelectionLabelFontDelta = 1;
const int kSelectedUsernameFontDelta = 1;
const int kUnselectedUsernameFontDelta = 2;
const int kWelcomeTitleFontDelta = 9;
const int kLoginTitleFontDelta = 4;
#endif

// New pod sizes.
const int kNewUserPodFullWidth = 372;
const int kNewUserPodFullHeight = 372;
const int kNewUserPodSmallWidth = 360;
const int kNewUserPodSmallHeight = 322;

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_HELPER_H_
