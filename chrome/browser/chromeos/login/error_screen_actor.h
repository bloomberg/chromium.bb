// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ERROR_SCREEN_ACTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ERROR_SCREEN_ACTOR_H_

#include "chrome/browser/chromeos/cros/network_constants.h"
#include "chrome/browser/chromeos/login/error_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

class ErrorScreenActor {
 public:
  // Possible error reasons.
  static const char kErrorReasonProxyAuthCancelled[];
  static const char kErrorReasonProxyAuthSupplied[];
  static const char kErrorReasonProxyConnectionFailed[];
  static const char kErrorReasonProxyConfigChanged[];
  static const char kErrorReasonLoadingTimeout[];
  static const char kErrorReasonPortalDetected[];
  // Reason for a case when network manager notifies about network
  // change.
  static const char kErrorReasonNetworkChanged[];
  // Reason for a case when JS side requires error screen update.
  static const char kErrorReasonUpdate[];

  ErrorScreenActor();
  virtual ~ErrorScreenActor();

  ErrorScreen::UIState ui_state() const { return ui_state_; }
  ErrorScreen::ErrorState error_state() const { return error_state_; }

  // Returns id of the screen behind error screen ("caller" screen).
  // Returns OobeUI::SCREEN_UNKNOWN if error screen isn't the current
  // screen.
  OobeUI::Screen parent_screen() const { return parent_screen_; }

  // Shows the screen.
  virtual void Show(OobeDisplay::Screen parent_screen,
                    base::DictionaryValue* params) = 0;

  // Hides the screen.
  virtual void Hide() = 0;

  // Initializes captive portal dialog and shows that if needed.
  virtual void FixCaptivePortal() = 0;

  // Shows captive portal dialog.
  virtual void ShowCaptivePortal() = 0;

  // Hides captive portal dialog.
  virtual void HideCaptivePortal() = 0;

  virtual void SetUIState(ErrorScreen::UIState ui_state) = 0;
  virtual void SetErrorState(ErrorScreen::ErrorState error_state,
                             const std::string& network) = 0;

  virtual void AllowGuestSignin(bool allowed) = 0;
  virtual void AllowOfflineLogin(bool allowed) = 0;

 protected:
  ErrorScreen::UIState ui_state_;
  ErrorScreen::ErrorState error_state_;
  OobeUI::Screen parent_screen_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ERROR_SCREEN_ACTOR_H_
