// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_VIEWS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_VIEWS_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chromeos/login/auth/auth_status_consumer.h"

namespace chromeos {

class ExistingUserController;

// A LoginDisplayHost instance that sends requests to the views-based signin
// screen.
class LoginDisplayHostViews : public LoginDisplayHost,
                              public LoginScreenClient::Delegate,
                              public AuthStatusConsumer {
 public:
  LoginDisplayHostViews();
  ~LoginDisplayHostViews() override;

  // LoginDisplayHost:
  LoginDisplay* CreateLoginDisplay(LoginDisplay::Delegate* delegate) override;
  gfx::NativeWindow GetNativeWindow() const override;
  OobeUI* GetOobeUI() const override;
  WebUILoginView* GetWebUILoginView() const override;
  void BeforeSessionStart() override;
  void Finalize(base::OnceClosure completion_callback) override;
  void SetStatusAreaVisible(bool visible) override;
  void StartWizard(OobeScreen first_screen) override;
  WizardController* GetWizardController() override;
  void StartUserAdding(base::OnceClosure completion_callback) override;
  void CancelUserAdding() override;
  void OnStartSignInScreen(const LoginScreenContext& context) override;
  void OnPreferencesChanged() override;
  void OnStartAppLaunch() override;
  void StartArcKiosk(const AccountId& account_id) override;
  void StartVoiceInteractionOobe() override;
  bool IsVoiceInteractionOobe() override;

  // LoginScreenClient::Delegate:
  void HandleAuthenticateUser(const AccountId& account_id,
                              const std::string& hashed_password,
                              bool authenticated_by_pin,
                              AuthenticateUserCallback callback) override;
  void HandleAttemptUnlock(const AccountId& account_id) override;
  void HandleHardlockPod(const AccountId& account_id) override;
  void HandleRecordClickOnLockIcon(const AccountId& account_id) override;
  void HandleOnFocusPod(const AccountId& account_id) override;
  void HandleOnNoPodFocused() override;
  bool HandleFocusLockScreenApps(bool reverse) override;

  // AuthStatusConsumer:
  void OnAuthFailure(const AuthFailure& error) override;
  void OnAuthSuccess(const UserContext& user_context) override;

 private:
  // Callback that should be executed the authentication result is available.
  AuthenticateUserCallback on_authenticated_;

  std::unique_ptr<ExistingUserController> existing_user_controller_;
  base::WeakPtrFactory<LoginDisplayHostViews> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LoginDisplayHostViews);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_VIEWS_H_
