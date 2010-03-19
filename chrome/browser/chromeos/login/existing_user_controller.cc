// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/existing_user_controller.h"

#include <algorithm>
#include <functional>

#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/background_view.h"
#include "chrome/browser/chromeos/login/utils.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "views/screen.h"
#include "views/widget/widget.h"

namespace chromeos {

namespace {

// Max number of users we'll show. The true max is the min of this and the
// number of windows that fit on the screen.
const size_t kMaxUsers = 6;

// Used to indicate no user has been selected.
const size_t kNoLogin = -1;

}  // namespace

ExistingUserController::ExistingUserController(
    const std::vector<UserManager::User>& users,
    const gfx::Rect& background_bounds)
    : background_bounds_(background_bounds),
      background_window_(NULL),
      background_view_(NULL),
      index_of_view_logging_in_(kNoLogin) {
  DCHECK(!users.empty());  // There must be at least one user when using
                           // ExistingUserController.

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
  controllers_.push_back(new UserController());
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

  if (CrosLibrary::EnsureLoaded())
    LoginLibrary::Get()->EmitLoginPromptReady();
}

ExistingUserController::~ExistingUserController() {
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
  if (message.type() != WmIpc::Message::CHROME_CREATE_GUEST_WINDOW)
    return;

  // WizardController takes care of deleting itself when done.
  WizardController* controller = new WizardController();
  controller->Init(std::string(), background_bounds_, false);
  controller->Show();

  // Give the background window to the controller.
  controller->OwnBackground(background_window_, background_view_);
  background_window_ = NULL;

  // And schedule us for deletion. We delay for a second as the window manager
  // is doing an animation with our windows.
  delete_timer_.Start(base::TimeDelta::FromSeconds(1), this,
                      &ExistingUserController::Delete);
}

void ExistingUserController::Login(UserController* source,
                                   const string16& password) {
  std::vector<UserController*>::const_iterator i =
      std::find(controllers_.begin(), controllers_.end(), source);
  DCHECK(i != controllers_.end());
  index_of_view_logging_in_ = i - controllers_.begin();

  authenticator_.reset(login_utils::CreateAuthenticator(this));
  authenticator_->Authenticate(
      controllers_[index_of_view_logging_in_]->user().email(),
      UTF16ToUTF8(password));

  // Disable clicking on other windows.
  chromeos::WmIpc::Message message(
      chromeos::WmIpc::Message::WM_SET_LOGIN_STATE);
  message.set_param(0, 0);
  chromeos::WmIpc::instance()->SendMessage(message);
}

void ExistingUserController::OnLoginFailure(const std::string error) {
  LOG(INFO) << "OnLoginFailure";

  // TODO(sky): alert the user in some way to the failure.

  controllers_[index_of_view_logging_in_]->SetPasswordEnabled(true);

  // Reenable clicking on other windows.
  chromeos::WmIpc::Message message(
      chromeos::WmIpc::Message::WM_SET_LOGIN_STATE);
  message.set_param(0, 1);
  chromeos::WmIpc::instance()->SendMessage(message);
}

void ExistingUserController::OnLoginSuccess(const std::string username,
                                            std::vector<std::string> cookies) {
  // Hide the login windows now.
  STLDeleteElements(&controllers_);

  background_window_->Close();

  chromeos::login_utils::CompleteLogin(username, cookies);

  // Delay deletion as we're on the stack.
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

}  // namespace chromeos
