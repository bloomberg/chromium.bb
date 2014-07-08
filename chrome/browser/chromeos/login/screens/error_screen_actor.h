// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ERROR_SCREEN_ACTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ERROR_SCREEN_ACTOR_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/screens/error_screen_actor_delegate.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

class ErrorScreenActor {
 public:
  // Possible network error reasons.
  enum ErrorReason {
    ERROR_REASON_PROXY_AUTH_CANCELLED = 0,
    ERROR_REASON_PROXY_AUTH_SUPPLIED,
    ERROR_REASON_PROXY_CONNECTION_FAILED,
    ERROR_REASON_PROXY_CONFIG_CHANGED,
    ERROR_REASON_LOADING_TIMEOUT,
    ERROR_REASON_PORTAL_DETECTED,
    // Reason for a case when default network has changed.
    ERROR_REASON_NETWORK_STATE_CHANGED,
    // Reason for a case when JS side requires error screen update.
    ERROR_REASON_UPDATE,
    ERROR_REASON_FRAME_ERROR
  };

  ErrorScreenActor();
  virtual ~ErrorScreenActor();

  ErrorScreen::UIState ui_state() const { return ui_state_; }
  ErrorScreen::ErrorState error_state() const { return error_state_; }

  // Returns id of the screen behind error screen ("caller" screen).
  // Returns OobeUI::SCREEN_UNKNOWN if error screen isn't the current
  // screen.
  OobeUI::Screen parent_screen() const { return parent_screen_; }

  // Sets screen this actor belongs to.
  virtual void SetDelegate(ErrorScreenActorDelegate* delegate) = 0;

  // Shows the screen.
  virtual void Show(OobeDisplay::Screen parent_screen,
                    base::DictionaryValue* params) = 0;

  // Shows the screen and call |on_hide| on hide.
  virtual void Show(OobeDisplay::Screen parent_screen,
                    base::DictionaryValue* params,
                    const base::Closure& on_hide) = 0;

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

  virtual void ShowConnectingIndicator(bool show) = 0;

  static const char* ErrorReasonString(ErrorReason reason);

 protected:
  ErrorScreen::UIState ui_state_;
  ErrorScreen::ErrorState error_state_;
  std::string network_;
  bool guest_signin_allowed_;
  bool offline_login_allowed_;
  bool show_connecting_indicator_;

  OobeUI::Screen parent_screen_;

  DISALLOW_COPY_AND_ASSIGN(ErrorScreenActor);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ERROR_SCREEN_ACTOR_H_
