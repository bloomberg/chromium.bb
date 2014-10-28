// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ERROR_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ERROR_SCREEN_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/error_screen_actor_delegate.h"
#include "chrome/browser/chromeos/login/ui/oobe_display.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chromeos/login/auth/login_performer.h"

namespace chromeos {

class BaseScreenDelegate;

// Controller for the error screen.
class ErrorScreen : public BaseScreen,
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
    ERROR_STATE_NONE,
    // States above are being logged to histograms.
    // Please keep ERROR_STATE_NONE as the last one of the histogram values.
    ERROR_STATE_KIOSK_ONLINE,
  };

  ErrorScreen(BaseScreenDelegate* base_screen_delegate,
              ErrorScreenActor* actor);
  virtual ~ErrorScreen();

  // BaseScreen implementation.
  virtual void PrepareToShow() override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual std::string GetName() const override;

  // ErrorScreenActorDelegate implementation:
  virtual void OnErrorShow() override;
  virtual void OnErrorHide() override;
  virtual void OnLaunchOobeGuestSession() override;
  virtual void OnActorDestroyed() override;

  // LoginPerformer::Delegate implementation:
  virtual void OnAuthFailure(const AuthFailure& error) override;
  virtual void OnAuthSuccess(const UserContext& user_context) override;
  virtual void OnOffTheRecordAuthSuccess() override;
  virtual void OnPasswordChangeDetected() override;
  virtual void WhiteListCheckFailed(const std::string& email) override;
  virtual void PolicyLoadFailed() override;
  virtual void OnOnlineChecked(const std::string& username,
                               bool success) override;

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

  ErrorState GetErrorState() const;

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
