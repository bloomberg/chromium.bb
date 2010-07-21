// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_screen.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/authentication_notification_details.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/message_bubble.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/notification_service.h"
#include "grit/theme_resources.h"

namespace chromeos {

LoginScreen::LoginScreen(WizardScreenDelegate* delegate)
    : ViewScreen<NewUserView>(delegate),
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
  return new NewUserView(this, true);
}

void LoginScreen::OnLogin(const std::string& username,
                          const std::string& password) {
  BootTimesLoader::Get()->RecordLoginAttempted();
  Profile* profile = g_browser_process->profile_manager()->GetDefaultProfile();
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(authenticator_.get(),
                        &Authenticator::AuthenticateToLogin,
                        profile, username, password,
                        std::string(), std::string()));
}

void LoginScreen::OnLoginOffTheRecord() {
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
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

void LoginScreen::OnLoginFailure(const std::string& error) {
  LOG(INFO) << "LoginManagerView: OnLoginFailure() " << error;
  NetworkLibrary* network = CrosLibrary::Get()->GetNetworkLibrary();

  // Check networking after trying to login in case user is
  // cached locally or the local admin account.
  if (!network || !CrosLibrary::Get()->EnsureLoaded()) {
    ShowError(IDS_LOGIN_ERROR_NO_NETWORK_LIBRARY, error);
  } else if (!network->Connected()) {
    ShowError(IDS_LOGIN_ERROR_OFFLINE_FAILED_NETWORK_NOT_CONNECTED, error);
  } else {
    ShowError(IDS_LOGIN_ERROR_AUTHENTICATING, error);
  }

  view()->ClearAndEnablePassword();
}

void LoginScreen::OnLoginSuccess(const std::string& username,
    const GaiaAuthConsumer::ClientLoginResult& credentials) {

  delegate()->GetObserver(this)->OnExit(ScreenObserver::LOGIN_SIGN_IN_SELECTED);
  AppendStartUrlToCmdline();
  LoginUtils::Get()->CompleteLogin(username, credentials);
}

void LoginScreen::OnOffTheRecordLoginSuccess() {
  delegate()->GetObserver(this)->OnExit(ScreenObserver::LOGIN_GUEST_SELECTED);
  AppendStartUrlToCmdline();
  LoginUtils::Get()->CompleteOffTheRecordLogin();
}

void LoginScreen::AppendStartUrlToCmdline() {
  if (start_url_.is_valid()) {
    CommandLine::ForCurrentProcess()->AppendLooseValue(
        UTF8ToWide(start_url_.spec()));
  }
}

void LoginScreen::ShowError(int error_id, const std::string& details) {
  ClearErrors();
  std::wstring error_text = l10n_util::GetString(error_id);
  if (!details.empty())
    error_text += L"\n" + ASCIIToWide(details);
  bubble_ = MessageBubble::Show(
      view()->GetWidget(),
      view()->GetPasswordBounds(),
      BubbleBorder::LEFT_TOP,
      ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_WARNING),
      error_text,
      this);
}

}  // namespace chromeos
