// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/error_screen.h"

#include "base/command_line.h"
#include "chrome/browser/chromeos/login/auth/chrome_login_performer.h"
#include "chrome/browser/chromeos/login/chrome_restart_request.h"
#include "chrome/browser/chromeos/login/screens/error_screen_actor.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"

namespace chromeos {

ErrorScreen::ErrorScreen(BaseScreenDelegate* base_screen_delegate,
                         ErrorScreenActor* actor)
    : BaseScreen(base_screen_delegate),
      actor_(actor),
      parent_screen_(OobeDisplay::SCREEN_UNKNOWN),
      weak_factory_(this) {
  CHECK(actor_);
  actor_->SetDelegate(this);
}

ErrorScreen::~ErrorScreen() {
  if (actor_)
    actor_->SetDelegate(NULL);
}

void ErrorScreen::PrepareToShow() {
}

void ErrorScreen::Show() {
  if (actor_)
    actor_->Show(parent_screen(), NULL);
}

void ErrorScreen::Hide() {
  if (actor_)
    actor_->Hide();
}

std::string ErrorScreen::GetName() const {
  return WizardController::kErrorScreenName;
}

void ErrorScreen::OnErrorShow() {}

void ErrorScreen::OnErrorHide() {}

void ErrorScreen::OnLaunchOobeGuestSession() {
  DeviceSettingsService::Get()->GetOwnershipStatusAsync(
      base::Bind(&ErrorScreen::StartGuestSessionAfterOwnershipCheck,
                 weak_factory_.GetWeakPtr()));
}

void ErrorScreen::OnActorDestroyed() {
  actor_ = nullptr;
}

void ErrorScreen::OnAuthFailure(const AuthFailure& error) {
  // The only condition leading here is guest mount failure, which should not
  // happen in practice. For now, just log an error so this situation is visible
  // in logs if it ever occurs.
  NOTREACHED() << "Guest login failed.";
  guest_login_performer_.reset();
}

void ErrorScreen::OnAuthSuccess(const UserContext& user_context) {
  LOG(FATAL);
}

void ErrorScreen::OnOffTheRecordAuthSuccess() {
  // Restart Chrome to enter the guest session.
  const base::CommandLine& browser_command_line =
      *base::CommandLine::ForCurrentProcess();
  base::CommandLine command_line(browser_command_line.GetProgram());
  std::string cmd_line_str =
      GetOffTheRecordCommandLine(GURL(),
                                 StartupUtils::IsOobeCompleted(),
                                 browser_command_line,
                                 &command_line);

  RestartChrome(cmd_line_str);
}

void ErrorScreen::OnPasswordChangeDetected() {
  LOG(FATAL);
}

void ErrorScreen::WhiteListCheckFailed(const std::string& email) {
  LOG(FATAL);
}

void ErrorScreen::PolicyLoadFailed() {
  LOG(FATAL);
}

void ErrorScreen::OnOnlineChecked(const std::string& username, bool success) {
  LOG(FATAL);
}

void ErrorScreen::FixCaptivePortal() {
  if (actor_)
    actor_->FixCaptivePortal();
}

void ErrorScreen::ShowCaptivePortal() {
  if (actor_)
    actor_->ShowCaptivePortal();
}

void ErrorScreen::HideCaptivePortal() {
  if (actor_)
    actor_->HideCaptivePortal();
}

void ErrorScreen::SetUIState(UIState ui_state) {
  if (actor_)
    actor_->SetUIState(ui_state);
}

ErrorScreen::UIState ErrorScreen::GetUIState() const {
  return actor_ ? actor_->ui_state() : UI_STATE_UNKNOWN;
}

void ErrorScreen::SetErrorState(ErrorState error_state,
                                const std::string& network) {
  if (actor_)
    actor_->SetErrorState(error_state, network);
}

ErrorScreen::ErrorState ErrorScreen::GetErrorState() const {
  DCHECK(actor_);
  return actor_->error_state();
}

void ErrorScreen::AllowGuestSignin(bool allow) {
  if (actor_)
    actor_->AllowGuestSignin(allow);
}

void ErrorScreen::ShowConnectingIndicator(bool show) {
  if (actor_)
    actor_->ShowConnectingIndicator(show);
}

void ErrorScreen::StartGuestSessionAfterOwnershipCheck(
    DeviceSettingsService::OwnershipStatus ownership_status) {

  // Make sure to disallow guest login if it's explicitly disabled.
  CrosSettingsProvider::TrustedStatus trust_status =
      CrosSettings::Get()->PrepareTrustedValues(
          base::Bind(&ErrorScreen::StartGuestSessionAfterOwnershipCheck,
                     weak_factory_.GetWeakPtr(),
                     ownership_status));
  switch (trust_status) {
    case CrosSettingsProvider::TEMPORARILY_UNTRUSTED:
      // Wait for a callback.
      return;
    case CrosSettingsProvider::PERMANENTLY_UNTRUSTED:
      // Only allow guest sessions if there is no owner yet.
      if (ownership_status == DeviceSettingsService::OWNERSHIP_NONE)
        break;
      return;
    case CrosSettingsProvider::TRUSTED: {
      // Honor kAccountsPrefAllowGuest.
      bool allow_guest = false;
      CrosSettings::Get()->GetBoolean(kAccountsPrefAllowGuest, &allow_guest);
      if (allow_guest)
        break;
      return;
    }
  }

  if (guest_login_performer_)
    return;

  guest_login_performer_.reset(new ChromeLoginPerformer(this));
  guest_login_performer_->LoginOffTheRecord();
}

}  // namespace chromeos
