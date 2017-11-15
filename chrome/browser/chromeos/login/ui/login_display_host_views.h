// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_VIEWS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_VIEWS_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"

namespace chromeos {

// A LoginDisplayHost instance that sends requests to the views-based signin
// screen.
class LoginDisplayHostViews : public LoginDisplayHost {
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
  void OpenInternetDetailDialog(const std::string& network_id) override;
  void SetStatusAreaVisible(bool visible) override;
  void StartWizard(OobeScreen first_screen) override;
  WizardController* GetWizardController() override;
  AppLaunchController* GetAppLaunchController() override;
  void StartUserAdding(base::OnceClosure completion_callback) override;
  void CancelUserAdding() override;
  void StartSignInScreen(const LoginScreenContext& context) override;
  void OnPreferencesChanged() override;
  void PrewarmAuthentication() override;
  void StartAppLaunch(const std::string& app_id,
                      bool diagnostic_mode,
                      bool is_auto_launch) override;
  void StartDemoAppLaunch() override;
  void StartArcKiosk(const AccountId& account_id) override;
  void StartVoiceInteractionOobe() override;
  bool IsVoiceInteractionOobe() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginDisplayHostViews);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_LOGIN_DISPLAY_HOST_VIEWS_H_
