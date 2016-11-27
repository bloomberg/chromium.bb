// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_ERROR_MODEL_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_ERROR_MODEL_H_

#include "base/callback_list.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/network_error.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_screen.h"

namespace chromeos {

class BaseScreenDelegate;
class NetworkErrorView;

class NetworkErrorModel : public BaseScreen {
 public:
  static const char kContextKeyErrorStateCode[];
  static const char kContextKeyErrorStateNetwork[];
  static const char kContextKeyGuestSigninAllowed[];
  static const char kContextKeyOfflineSigninAllowed[];
  static const char kContextKeyShowConnectingIndicator[];
  static const char kContextKeyUIState[];
  static const char kUserActionConfigureCertsButtonClicked[];
  static const char kUserActionDiagnoseButtonClicked[];
  static const char kUserActionLaunchOobeGuestSessionClicked[];
  static const char kUserActionLocalStateErrorPowerwashButtonClicked[];
  static const char kUserActionRebootButtonClicked[];
  static const char kUserActionShowCaptivePortalClicked[];
  static const char kUserActionConnectRequested[];

  explicit NetworkErrorModel(BaseScreenDelegate* base_screen_delegate);
  ~NetworkErrorModel() override;

  // BaseScreen:
  std::string GetName() const override;

  // Toggles the guest sign-in prompt.
  virtual void AllowGuestSignin(bool allowed) = 0;

  // Toggles the offline sign-in.
  virtual void AllowOfflineLogin(bool allowed) = 0;

  // Initializes captive portal dialog and shows that if needed.
  virtual void FixCaptivePortal() = 0;

  virtual NetworkError::UIState GetUIState() const = 0;
  virtual NetworkError::ErrorState GetErrorState() const = 0;

  // Returns id of the screen behind error screen ("caller" screen).
  // Returns OobeScreen::SCREEN_UNKNOWN if error screen isn't the current
  // screen.
  virtual OobeScreen GetParentScreen() const = 0;

  // Called when we're asked to hide captive portal dialog.
  virtual void HideCaptivePortal() = 0;

  // This method is called, when view is being destroyed. Note, if model
  // is destroyed earlier then it has to call Unbind().
  virtual void OnViewDestroyed(NetworkErrorView* view) = 0;

  // Sets current UI state.
  virtual void SetUIState(NetworkError::UIState ui_state) = 0;

  // Sets current error screen content according to current UI state,
  // |error_state|, and |network|.
  virtual void SetErrorState(NetworkError::ErrorState error_state,
                             const std::string& network) = 0;

  // Sets "parent screen" i.e. one that has initiated this network error screen
  // instance.
  virtual void SetParentScreen(OobeScreen parent_screen) = 0;

  // Sets callback that is called on hide.
  virtual void SetHideCallback(const base::Closure& on_hide) = 0;

  // Shows captive portal dialog.
  virtual void ShowCaptivePortal() = 0;

  // Toggles the connection pending indicator.
  virtual void ShowConnectingIndicator(bool show) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_ERROR_MODEL_H_
