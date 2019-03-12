// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_ERROR_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_ERROR_VIEW_H_

#include "base/strings/string16.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/network_error.h"

namespace chromeos {

class ErrorScreen;

// Interface for dependency injection between ErrorScreen and its actual
// representation. Owned by ErrorScreen.
class NetworkErrorView {
 public:
  constexpr static OobeScreen kScreenId = OobeScreen::SCREEN_ERROR_MESSAGE;

  virtual ~NetworkErrorView() {}

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Binds |screen| to the view.
  virtual void Bind(ErrorScreen* screen) = 0;

  // Unbinds the screen from the view.
  virtual void Unbind() = 0;

  // Switches to |screen|.
  virtual void ShowOobeScreen(OobeScreen screen) = 0;

  // Sets current error state of the screen.
  virtual void SetErrorStateCode(NetworkError::ErrorState error_state) = 0;

  // Sets current error network state of the screen.
  virtual void SetErrorStateNetwork(const std::string& network_name) = 0;

  // Is guest signin allowed?
  virtual void SetGuestSigninAllowed(bool value) = 0;

  // Is offline signin allowed?
  virtual void SetOfflineSigninAllowed(bool value) = 0;

  // Updates visibility of the label indicating we're reconnecting.
  virtual void SetShowConnectingIndicator(bool value) = 0;

  // Sets current UI state of the screen.
  virtual void SetUIState(NetworkError::UIState ui_state) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_NETWORK_ERROR_VIEW_H_
