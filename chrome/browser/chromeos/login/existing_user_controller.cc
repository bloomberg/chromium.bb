// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/existing_user_controller.h"

#include <algorithm>
#include <functional>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/background_view.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/message_bubble.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "grit/theme_resources.h"
#include "views/screen.h"
#include "views/widget/widget.h"

namespace chromeos {

namespace {

// Max number of users we'll show. The true max is the min of this and the
// number of windows that fit on the screen.
const size_t kMaxUsers = 6;

// Used to indicate no user has been selected.
const size_t kNotSelected = -1;

}  // namespace

ExistingUserController::ExistingUserController(
    const std::vector<UserManager::User>& users,
    const gfx::Rect& background_bounds)
    : background_bounds_(background_bounds),
      background_window_(NULL),
      background_view_(NULL),
      selected_view_index_(kNotSelected),
      bubble_(NULL) {
  // Caclulate the max number of users from available screen size.
  size_t max_users = kMaxUsers;
  int screen_width = background_bounds.width();
  if (screen_width > 0) {
    max_users = std::max(static_cast<size_t>(2), std::min(kMaxUsers,
        static_cast<size_t>((screen_width - UserController::kSize)
                            / (UserController::kUnselectedSize +
                               UserController::kPadding))));
  }

  for (size_t i = 0, max = std::min(users.size(), max_users - 1); i < max;
       ++i) {
    controllers_.push_back(new UserController(this, users[i]));
  }

  // Add the view representing the guest user last.
  controllers_.push_back(new UserController(this));
}

void ExistingUserController::Init() {
  background_window_ = BackgroundView::CreateWindowContainingView(
      background_bounds_,
      &background_view_);
  background_window_->Show();
  for (size_t i = 0; i < controllers_.size(); ++i) {
    (controllers_[i])->Init(static_cast<int>(i),
                            static_cast<int>(controllers_.size()));
  }

  WmMessageListener::instance()->AddObserver(this);

  if (CrosLibrary::Get()->EnsureLoaded())
    CrosLibrary::Get()->GetLoginLibrary()->EmitLoginPromptReady();
}

ExistingUserController::~ExistingUserController() {
  ClearErrors();

  if (background_window_)
    background_window_->Close();

  WmMessageListener::instance()->RemoveObserver(this);

  STLDeleteElements(&controllers_);
}

void ExistingUserController::Delete() {
  delete this;
}

void ExistingUserController::ProcessWmMessage(const WmIpc::Message& message,
                                              GdkWindow* window) {
  if (message.type() != WM_IPC_MESSAGE_CHROME_CREATE_GUEST_WINDOW)
    return;

  ActivateWizard(std::string());
}

void ExistingUserController::SendSetLoginState(bool is_enabled) {
  WmIpc::Message message(WM_IPC_MESSAGE_WM_SET_LOGIN_STATE);
  message.set_param(0, is_enabled);
  WmIpc::instance()->SendMessage(message);
}

void ExistingUserController::Login(UserController* source,
                                   const string16& password) {
  std::vector<UserController*>::const_iterator i =
      std::find(controllers_.begin(), controllers_.end(), source);
  DCHECK(i != controllers_.end());
  selected_view_index_ = i - controllers_.begin();

  authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
  Profile* profile = g_browser_process->profile_manager()->GetDefaultProfile();
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(authenticator_.get(),
                        &Authenticator::AuthenticateToLogin,
                        profile,
                        controllers_[selected_view_index_]->user().email(),
                        UTF16ToUTF8(password)));

  // Disable clicking on other windows.
  SendSetLoginState(false);
}

void ExistingUserController::LoginOffTheRecord() {
  authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(authenticator_.get(),
                        &Authenticator::LoginOffTheRecord));

  // Disable clicking on other windows.
  SendSetLoginState(false);
}

void ExistingUserController::ClearErrors() {
  // bubble_ will be set to NULL in callback.
  if (bubble_)
    bubble_->Close();
}

void ExistingUserController::OnUserSelected(UserController* source) {
  std::vector<UserController*>::const_iterator i =
      std::find(controllers_.begin(), controllers_.end(), source);
  DCHECK(i != controllers_.end());
  size_t new_selected_index = i - controllers_.begin();
  if (new_selected_index != selected_view_index_ &&
      selected_view_index_ != kNotSelected) {
    controllers_[selected_view_index_]->ClearAndEnablePassword();
  }
  selected_view_index_ = new_selected_index;
}

void ExistingUserController::ActivateWizard(const std::string& screen_name) {
  // WizardController takes care of deleting itself when done.
  WizardController* controller = new WizardController();
  controller->Init(screen_name, background_bounds_, false);
  controller->Show();

  // Give the background window to the controller.
  controller->OwnBackground(background_window_, background_view_);
  background_window_ = NULL;

  // And schedule us for deletion. We delay for a second as the window manager
  // is doing an animation with our windows.
  delete_timer_.Start(base::TimeDelta::FromSeconds(1), this,
                      &ExistingUserController::Delete);
}

void ExistingUserController::OnLoginFailure(const std::string& error) {
  LOG(INFO) << "OnLoginFailure";

  // Check networking after trying to login in case user is
  // cached locally or the local admin account.
  NetworkLibrary* network = CrosLibrary::Get()->GetNetworkLibrary();
  if (!network || !CrosLibrary::Get()->EnsureLoaded()) {
    ShowError(IDS_LOGIN_ERROR_NO_NETWORK_LIBRARY, error);
  } else if (!network->Connected()) {
    ShowError(IDS_LOGIN_ERROR_OFFLINE_FAILED_NETWORK_NOT_CONNECTED, error);
  } else {
    ShowError(IDS_LOGIN_ERROR_AUTHENTICATING, error);
  }

  controllers_[selected_view_index_]->ClearAndEnablePassword();

  // Reenable clicking on other windows.
  SendSetLoginState(true);
}

void ExistingUserController::ShowError(int error_id,
                                       const std::string& details) {
  ClearErrors();
  std::wstring error_text = l10n_util::GetString(error_id);
  if (!details.empty())
    error_text += L"\n" + ASCIIToWide(details);
  bubble_ = MessageBubble::Show(
      controllers_[selected_view_index_]->controls_window(),
      controllers_[selected_view_index_]->GetScreenBounds(),
      BubbleBorder::BOTTOM_LEFT,
      ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_WARNING),
      error_text,
      this);
}

void ExistingUserController::OnLoginSuccess(const std::string& username,
                                            const std::string& credentials) {
  if (selected_view_index_ + 1 == controllers_.size()) {
    // For new user login don't launch browser until we pass image screen.
    chromeos::LoginUtils::Get()->EnableBrowserLaunch(false);
    LoginUtils::Get()->CompleteLogin(username, credentials);
    ActivateWizard(WizardController::kUserImageScreenName);
  } else {
    // Hide the login windows now.
    STLDeleteElements(&controllers_);

    background_window_->Close();

    LoginUtils::Get()->CompleteLogin(username, credentials);
    // Delay deletion as we're on the stack.
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }
}

void ExistingUserController::OnOffTheRecordLoginSuccess() {
  LoginUtils::Get()->CompleteOffTheRecordLogin();
}

}  // namespace chromeos
