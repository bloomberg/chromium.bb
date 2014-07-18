// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ERROR_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ERROR_SCREEN_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/auth/login_performer.h"
#include "chrome/browser/chromeos/login/screens/error_screen_actor_delegate.h"
#include "chrome/browser/chromeos/login/screens/wizard_screen.h"
#include "chrome/browser/chromeos/login/ui/oobe_display.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"

namespace chromeos {

class ScreenObserver;

// Controller for the error screen.
class ErrorScreen : public WizardScreen,
                    public ErrorScreenActorDelegate,
                    public LoginPerformer::Delegate {
 public:
  enum UIState {
    UI_STATE_UNKNOWN = 0,
    UI_STATE_UPDATE,
    UI_STATE_SIGNIN,
    UI_STATE_SUPERVISED,
    UI_STATE_KIOSK_MODE,
    UI_STATE_LOCAL_STATE_ERROR,
    UI_STATE_AUTO_ENROLLMENT_ERROR,
    UI_STATE_ROLLBACK_ERROR,
  };

  enum ErrorState {
    ERROR_STATE_UNKNOWN = 0,
    ERROR_STATE_PORTAL,
    ERROR_STATE_OFFLINE,
    ERROR_STATE_PROXY,
    ERROR_STATE_AUTH_EXT_TIMEOUT,
    ERROR_STATE_KIOSK_ONLINE
  };

  ErrorScreen(ScreenObserver* screen_observer, ErrorScreenActor* actor);
  virtual ~ErrorScreen();

  // WizardScreen implementation.
  virtual void PrepareToShow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual std::string GetName() const OVERRIDE;

  // ErrorScreenActorDelegate implementation:
  virtual void OnErrorShow() OVERRIDE;
  virtual void OnErrorHide() OVERRIDE;
  virtual void OnLaunchOobeGuestSession() OVERRIDE;

  // LoginPerformer::Delegate implementation:
  virtual void OnAuthFailure(const AuthFailure& error) OVERRIDE;
  virtual void OnAuthSuccess(const UserContext& user_context) OVERRIDE;
  virtual void OnOffTheRecordAuthSuccess() OVERRIDE;
  virtual void OnPasswordChangeDetected() OVERRIDE;
  virtual void WhiteListCheckFailed(const std::string& email) OVERRIDE;
  virtual void PolicyLoadFailed() OVERRIDE;
  virtual void OnOnlineChecked(const std::string& username,
                               bool success) OVERRIDE;

  // Initializes captive portal dialog and shows that if needed.
  void FixCaptivePortal();

  // Shows captive portal dialog.
  void ShowCaptivePortal();

  // Hides captive portal dialog.
  void HideCaptivePortal();

  // Sets current UI state.
  void SetUIState(UIState ui_state);

  UIState GetUIState() const;

  // Sets current error screen content according to current UI state,
  // |error_state|, and |network|.
  void SetErrorState(ErrorState error_state, const std::string& network);

  // Toggles the guest sign-in prompt.
  void AllowGuestSignin(bool allow);

  // Toggles the connection pending indicator.
  void ShowConnectingIndicator(bool show);

  void set_parent_screen(OobeDisplay::Screen parent_screen) {
    parent_screen_ = parent_screen;
  }
  OobeDisplay::Screen parent_screen() const { return parent_screen_; }

 private:
  // Handles the response of an ownership check and starts the guest session if
  // applicable.
  void StartGuestSessionAfterOwnershipCheck(
      DeviceSettingsService::OwnershipStatus ownership_status);

  ErrorScreenActor* actor_;

  OobeDisplay::Screen parent_screen_;

  scoped_ptr<LoginPerformer> guest_login_performer_;

  base::WeakPtrFactory<ErrorScreen> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ErrorScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ERROR_SCREEN_H_
