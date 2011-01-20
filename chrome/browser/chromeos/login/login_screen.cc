// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_screen.h"

#include "app/l10n_util.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/authentication_notification_details.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/message_bubble.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace chromeos {

LoginScreen::LoginScreen(WizardScreenDelegate* delegate)
    : ViewScreen<NewUserView>(delegate,
          kNewUserPodFullWidth, kNewUserPodFullHeight),
      bubble_(NULL),
      authenticator_(NULL) {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
  }
}

LoginScreen::~LoginScreen() {
  ClearErrors();
}

NewUserView* LoginScreen::AllocateView() {
  return new NewUserView(this, true, true);
}

void LoginScreen::OnLogin(const std::string& username,
                          const std::string& password) {
  BootTimesLoader::Get()->RecordLoginAttempted();
  Profile* profile = g_browser_process->profile_manager()->GetDefaultProfile();
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(authenticator_.get(),
                        &Authenticator::AuthenticateToLogin,
                        profile, username, password,
                        std::string(), std::string()));
}

void LoginScreen::OnLoginOffTheRecord() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(authenticator_.get(),
                        &Authenticator::LoginOffTheRecord));
}

void LoginScreen::OnCreateAccount() {
  delegate()->GetObserver(this)->OnExit(ScreenObserver::LOGIN_CREATE_ACCOUNT);
}

void LoginScreen::ClearErrors() {
  // bubble_ will be set to NULL in InfoBubbleClosing callback.
  if (bubble_)
    bubble_->Close();
}

void LoginScreen::OnLoginFailure(const LoginFailure& failure) {
  const std::string error = failure.GetErrorString();
  VLOG(1) << "LoginManagerView: OnLoginFailure() " << error;
  NetworkLibrary* network = CrosLibrary::Get()->GetNetworkLibrary();

  // Check networking after trying to login in case user is
  // cached locally or the local admin account.
  if (!network || !CrosLibrary::Get()->EnsureLoaded()) {
    ShowError(IDS_LOGIN_ERROR_NO_NETWORK_LIBRARY, error);
  } else if (!network->Connected()) {
    ShowError(IDS_LOGIN_ERROR_OFFLINE_FAILED_NETWORK_NOT_CONNECTED, error);
  } else {
    ShowError(IDS_LOGIN_ERROR_AUTHENTICATING_NEW, error);
  }

  view()->ClearAndFocusPassword();
  view()->EnableInputControls(true);
}

void LoginScreen::OnLoginSuccess(
    const std::string& username,
    const std::string& password,
    const GaiaAuthConsumer::ClientLoginResult& credentials,
    bool pending_requests) {

  delegate()->GetObserver(this)->OnExit(ScreenObserver::LOGIN_SIGN_IN_SELECTED);
  AppendStartUrlToCmdline();
  LoginUtils::Get()->CompleteLogin(username,
                                   password,
                                   credentials,
                                   pending_requests);
}

void LoginScreen::OnOffTheRecordLoginSuccess() {
  LoginUtils::Get()->CompleteOffTheRecordLogin(start_url_);
}

void LoginScreen::OnHelpLinkActivated() {
  AddStartUrl(GetAccountRecoveryHelpUrl());
  OnLoginOffTheRecord();
}

void LoginScreen::AppendStartUrlToCmdline() {
  if (start_url_.is_valid())
    CommandLine::ForCurrentProcess()->AppendArg(start_url_.spec());
}

void LoginScreen::ShowError(int error_id, const std::string& details) {
  ClearErrors();
  std::wstring error_text = UTF16ToWide(l10n_util::GetStringUTF16(error_id));
  // TODO(dpolukhin): show detailed error info. |details| string contains
  // low level error info that is not localized and even is not user friendly.
  // For now just ignore it because error_text contains all required information
  // for end users, developers can see details string in Chrome logs.
  bubble_ = MessageBubble::Show(
      view()->GetWidget(),
      view()->GetPasswordBounds(),
      BubbleBorder::LEFT_TOP,
      ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_WARNING),
      error_text,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_CANT_ACCESS_ACCOUNT_BUTTON)),
      this);
}

}  // namespace chromeos
