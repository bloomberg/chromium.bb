// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_display_host_views.h"

namespace chromeos {

LoginDisplayHostViews::LoginDisplayHostViews() = default;

LoginDisplayHostViews::~LoginDisplayHostViews() = default;

LoginDisplay* LoginDisplayHostViews::CreateLoginDisplay(
    LoginDisplay::Delegate* delegate) {
  NOTREACHED();
  return nullptr;
}

gfx::NativeWindow LoginDisplayHostViews::GetNativeWindow() const {
  NOTREACHED();
  return nullptr;
}

OobeUI* LoginDisplayHostViews::GetOobeUI() const {
  NOTREACHED();
  return nullptr;
}

WebUILoginView* LoginDisplayHostViews::GetWebUILoginView() const {
  NOTREACHED();
  return nullptr;
}

void LoginDisplayHostViews::BeforeSessionStart() {
  NOTIMPLEMENTED();
}

void LoginDisplayHostViews::Finalize(base::OnceClosure completion_callback) {
  NOTIMPLEMENTED();
}

void LoginDisplayHostViews::OpenInternetDetailDialog(
    const std::string& network_id) {
  NOTREACHED();
}

void LoginDisplayHostViews::SetStatusAreaVisible(bool visible) {
  NOTIMPLEMENTED();
}

void LoginDisplayHostViews::StartWizard(OobeScreen first_screen) {
  NOTIMPLEMENTED();
}

WizardController* LoginDisplayHostViews::GetWizardController() {
  NOTIMPLEMENTED();
  return nullptr;
}

AppLaunchController* LoginDisplayHostViews::GetAppLaunchController() {
  NOTIMPLEMENTED();
  return nullptr;
}

void LoginDisplayHostViews::StartUserAdding(
    base::OnceClosure completion_callback) {
  NOTIMPLEMENTED();
}

void LoginDisplayHostViews::CancelUserAdding() {
  NOTIMPLEMENTED();
}

void LoginDisplayHostViews::StartSignInScreen(
    const LoginScreenContext& context) {
  NOTIMPLEMENTED();
}

void LoginDisplayHostViews::OnPreferencesChanged() {
  NOTIMPLEMENTED();
}

void LoginDisplayHostViews::PrewarmAuthentication() {
  NOTIMPLEMENTED();
}

void LoginDisplayHostViews::StartAppLaunch(const std::string& app_id,
                                           bool diagnostic_mode,
                                           bool is_auto_launch) {
  NOTIMPLEMENTED();
}

void LoginDisplayHostViews::StartDemoAppLaunch() {
  NOTIMPLEMENTED();
}

void LoginDisplayHostViews::StartArcKiosk(const AccountId& account_id) {
  NOTIMPLEMENTED();
}

void LoginDisplayHostViews::StartVoiceInteractionOobe() {
  NOTIMPLEMENTED();
}

bool LoginDisplayHostViews::IsVoiceInteractionOobe() {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace chromeos
