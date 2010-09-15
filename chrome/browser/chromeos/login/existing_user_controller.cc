// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/existing_user_controller.h"

#include <algorithm>
#include <functional>
#include <map>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/authenticator.h"
#include "chrome/browser/chromeos/login/background_view.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/message_bubble.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "gfx/native_widget_types.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/screen.h"
#include "views/widget/widget_gtk.h"
#include "views/window/window.h"

namespace chromeos {

namespace {

// Max number of users we'll show. The true max is the min of this and the
// number of windows that fit on the screen.
const size_t kMaxUsers = 6;

// Used to indicate no user has been selected.
const size_t kNotSelected = -1;

// Offset of cursor in first position from edit left side. It's used to position
// info bubble arrow to cursor.
const int kCursorOffset = 5;

// Checks if display names are unique. If there are duplicates, enables
// tooltips with full emails to let users distinguish their accounts.
// Otherwise, disables the tooltips.
void EnableTooltipsIfNeeded(const std::vector<UserController*>& controllers) {
  std::map<std::string, int> visible_display_names;
  for (size_t i = 0; i + 1 < controllers.size(); ++i) {
    const std::string& display_name =
        controllers[i]->user().GetDisplayName();
    ++visible_display_names[display_name];
  }
  for (size_t i = 0; i + 1 < controllers.size(); ++i) {
    const std::string& display_name =
        controllers[i]->user().GetDisplayName();
    bool show_tooltip = visible_display_names[display_name] > 1;
    controllers[i]->EnableNameTooltip(show_tooltip);
  }
}

}  // namespace

ExistingUserController*
  ExistingUserController::delete_scheduled_instance_ = NULL;

ExistingUserController::ExistingUserController(
    const std::vector<UserManager::User>& users,
    const gfx::Rect& background_bounds)
    : background_bounds_(background_bounds),
      background_window_(NULL),
      background_view_(NULL),
      selected_view_index_(kNotSelected),
      num_login_attempts_(0),
      bubble_(NULL),
      last_login_error_(GoogleServiceAuthError::NONE),
      login_timed_out_(false) {
  if (delete_scheduled_instance_)
    delete_scheduled_instance_->Delete();

  // Caclulate the max number of users from available screen size.
  size_t max_users = kMaxUsers;
  int screen_width = background_bounds.width();
  if (screen_width > 0) {
    max_users = std::max(static_cast<size_t>(2), std::min(kMaxUsers,
        static_cast<size_t>((screen_width - login::kUserImageSize)
                            / (UserController::kUnselectedSize +
                               UserController::kPadding))));
  }

  size_t visible_users_count = std::min(users.size(), max_users - 1);
  for (size_t i = 0; i < visible_users_count; ++i)
    controllers_.push_back(new UserController(this, users[i]));

  // Add the view representing the guest user last.
  controllers_.push_back(new UserController(this));
}

void ExistingUserController::Init() {
  if (!background_window_) {
    background_window_ = BackgroundView::CreateWindowContainingView(
        background_bounds_,
        &background_view_);

    if (!WizardController::IsDeviceRegistered()) {
      background_view_->SetOobeProgressBarVisible(true);
      background_view_->SetOobeProgress(chromeos::BackgroundView::SIGNIN);
    } else {
      background_view_->SetGoIncognitoButtonVisible(true, this);
    }

    background_window_->Show();
  }
  for (size_t i = 0; i < controllers_.size(); ++i) {
    (controllers_[i])->Init(static_cast<int>(i),
                            static_cast<int>(controllers_.size()),
                            background_view_->IsOobeProgressBarVisible());
  }

  EnableTooltipsIfNeeded(controllers_);

  WmMessageListener::instance()->AddObserver(this);

  if (CrosLibrary::Get()->EnsureLoaded())
    CrosLibrary::Get()->GetLoginLibrary()->EmitLoginPromptReady();
}

void ExistingUserController::OwnBackground(
    views::Widget* background_widget,
    chromeos::BackgroundView* background_view) {
  DCHECK(!background_window_);
  background_window_ = background_widget;
  background_view_ = background_view;
  background_view_->OnOwnerChanged();
}

void ExistingUserController::LoginNewUser(const std::string& username,
                                          const std::string& password) {
  SelectNewUser();
  UserController* new_user = controllers_.back();
  if (!new_user->is_guest())
    return;
  NewUserView* new_user_view = new_user->new_user_view();
  new_user_view->SetUsername(username);

  if (password.empty())
    return;

  new_user_view->SetPassword(password);
  LOG(INFO) << "Password: " << password;
  new_user_view->Login();
}

void ExistingUserController::SelectNewUser() {
  SelectUser(controllers_.size() - 1);
}

ExistingUserController::~ExistingUserController() {
  ClearErrors();

  if (background_window_)
    background_window_->Close();

  WmMessageListener::instance()->RemoveObserver(this);

  STLDeleteElements(&controllers_);
}

void ExistingUserController::Delete() {
  delete_scheduled_instance_ = NULL;
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
  BootTimesLoader::Get()->RecordLoginAttempted();
  std::vector<UserController*>::const_iterator i =
      std::find(controllers_.begin(), controllers_.end(), source);
  DCHECK(i != controllers_.end());

  if (i == controllers_.begin() + selected_view_index_) {
    num_login_attempts_++;
  } else {
    selected_view_index_ = i - controllers_.begin();
    num_login_attempts_ = 0;
  }

  authenticator_ = LoginUtils::Get()->CreateAuthenticator(this);
  Profile* profile = g_browser_process->profile_manager()->GetDefaultProfile();
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(authenticator_.get(),
                        &Authenticator::AuthenticateToLogin,
                        profile,
                        controllers_[selected_view_index_]->user().email(),
                        UTF16ToUTF8(password),
                        login_token_,
                        login_captcha_));

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
    controllers_[selected_view_index_]->ClearAndEnableFields();
    ClearCaptchaState();
    num_login_attempts_ = 0;
  }
  selected_view_index_ = new_selected_index;
}

void ExistingUserController::ActivateWizard(const std::string& screen_name) {
  // WizardController takes care of deleting itself when done.
  WizardController* controller = new WizardController();

  // Give the background window to the controller.
  controller->OwnBackground(background_window_, background_view_);
  background_window_ = NULL;

  controller->Init(screen_name, background_bounds_);
  controller->set_start_url(start_url_);
  controller->Show();

  // And schedule us for deletion. We delay for a second as the window manager
  // is doing an animation with our windows.
  DCHECK(!delete_scheduled_instance_);
  delete_scheduled_instance_ = this;
  delete_timer_.Start(base::TimeDelta::FromSeconds(1), this,
                      &ExistingUserController::Delete);
}

void ExistingUserController::RemoveUser(UserController* source) {
  ClearErrors();

  UserManager::Get()->RemoveUser(source->user().email());

  // We need to unmap entry windows, the windows will be unmapped in destructor.
  controllers_.erase(controllers_.begin() + source->user_index());
  delete source;

  EnableTooltipsIfNeeded(controllers_);
  int new_size = static_cast<int>(controllers_.size());
  for (int i = 0; i < new_size; ++i)
    controllers_[i]->UpdateUserCount(i, new_size);
}

void ExistingUserController::SelectUser(int index) {
  if (index >= 0 && index < static_cast<int>(controllers_.size()) &&
      index != static_cast<int>(selected_view_index_)) {
    WmIpc::Message message(WM_IPC_MESSAGE_WM_SELECT_LOGIN_USER);
    message.set_param(0, index);
    WmIpc::instance()->SendMessage(message);
  }
}

void ExistingUserController::OnGoIncognitoButton() {
  LoginOffTheRecord();
}

void ExistingUserController::OnLoginFailure(const LoginFailure& failure) {
  LOG(INFO) << "OnLoginFailure";
  ClearCaptchaState();

  last_login_error_ = failure.error();
  login_timed_out_ = failure.reason() == LoginFailure::LOGIN_TIMED_OUT;
  std::string error = failure.GetErrorString();

  // Check networking after trying to login in case user is
  // cached locally or the local admin account.
  NetworkLibrary* network = CrosLibrary::Get()->GetNetworkLibrary();
  if (!network || !CrosLibrary::Get()->EnsureLoaded()) {
    ShowError(IDS_LOGIN_ERROR_NO_NETWORK_LIBRARY, error);
  } else if (!network->Connected()) {
    ShowError(IDS_LOGIN_ERROR_OFFLINE_FAILED_NETWORK_NOT_CONNECTED, error);
  } else {
    if (failure.reason() == LoginFailure::NETWORK_AUTH_FAILED &&
        failure.error().state() == GoogleServiceAuthError::CAPTCHA_REQUIRED) {
      if (!failure.error().captcha().image_url.is_empty()) {
        // Save token for next login retry.
        login_token_ = failure.error().captcha().token;
        CaptchaView* view =
            new CaptchaView(failure.error().captcha().image_url);
        view->set_delegate(this);
        views::Window* window = views::Window::CreateChromeWindow(
            GetNativeWindow(), gfx::Rect(), view);
        window->SetIsAlwaysOnTop(true);
        window->Show();
      } else {
        LOG(WARNING) << "No captcha image url was found?";
        ShowError(IDS_LOGIN_ERROR_AUTHENTICATING, error);
      }
    } else {
      if (controllers_[selected_view_index_]->is_guest())
        ShowError(IDS_LOGIN_ERROR_AUTHENTICATING_NEW, error);
      else
        ShowError(IDS_LOGIN_ERROR_AUTHENTICATING, error);
    }
  }

  controllers_[selected_view_index_]->ClearAndEnablePassword();

  // Reenable clicking on other windows.
  SendSetLoginState(true);
}

void ExistingUserController::AppendStartUrlToCmdline() {
  if (start_url_.is_valid())
    CommandLine::ForCurrentProcess()->AppendArg(start_url_.spec());
}

void ExistingUserController::ClearCaptchaState() {
  login_token_.clear();
  login_captcha_.clear();
}

gfx::NativeWindow ExistingUserController::GetNativeWindow() const {
  return GTK_WINDOW(
      static_cast<views::WidgetGtk*>(background_window_)->GetNativeView());
}

void ExistingUserController::ShowError(int error_id,
                                       const std::string& details) {
  ClearErrors();
  std::wstring error_text = l10n_util::GetString(error_id);
  // TODO(dpolukhin): show detailed error info. |details| string contains
  // low level error info that is not localized and even is not user friendly.
  // For now just ignore it because error_text contains all required information
  // for end users, developers can see details string in Chrome logs.

  gfx::Rect bounds = controllers_[selected_view_index_]->GetScreenBounds();
  BubbleBorder::ArrowLocation arrow;
  if (controllers_[selected_view_index_]->is_guest()) {
    arrow = BubbleBorder::LEFT_TOP;
  } else {
    // Point info bubble arrow to cursor position (approximately).
    bounds.set_width(kCursorOffset * 2);
    arrow = BubbleBorder::BOTTOM_LEFT;
  }
  std::wstring help_link;
  if (num_login_attempts_ > static_cast<size_t>(1))
    help_link = l10n_util::GetString(IDS_CANT_ACCESS_ACCOUNT_BUTTON);

  bubble_ = MessageBubble::Show(
      controllers_[selected_view_index_]->controls_window(),
      bounds,
      arrow,
      ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_WARNING),
      error_text,
      help_link,
      this);
}

void ExistingUserController::OnLoginSuccess(const std::string& username,
    const GaiaAuthConsumer::ClientLoginResult& credentials) {

  AppendStartUrlToCmdline();
  if (selected_view_index_ + 1 == controllers_.size()) {
    // For new user login don't launch browser until we pass image screen.
    LoginUtils::Get()->EnableBrowserLaunch(false);
    LoginUtils::Get()->CompleteLogin(username, credentials);
    ActivateWizard(WizardController::IsDeviceRegistered() ?
        WizardController::kUserImageScreenName :
        WizardController::kRegistrationScreenName);
  } else {
    // Hide the login windows now.
    WmIpc::Message message(WM_IPC_MESSAGE_WM_HIDE_LOGIN);
    WmIpc::instance()->SendMessage(message);

    LoginUtils::Get()->CompleteLogin(username, credentials);

    // Delay deletion as we're on the stack.
    MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  }
}

void ExistingUserController::OnOffTheRecordLoginSuccess() {
  if (WizardController::IsDeviceRegistered()) {
    LoginUtils::Get()->CompleteOffTheRecordLogin(start_url_);
  } else {
    // Postpone CompleteOffTheRecordLogin until registration completion.
    ActivateWizard(WizardController::kRegistrationScreenName);
  }
}

void ExistingUserController::OnPasswordChangeDetected(
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  // When signing in as a "New user" always remove old cryptohome.
  if (selected_view_index_ == controllers_.size() - 1) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(authenticator_.get(),
                          &Authenticator::ResyncEncryptedData,
                          credentials));
    return;
  }

  cached_credentials_ = credentials;
  PasswordChangedView* view = new PasswordChangedView(this);
  views::Window* window = views::Window::CreateChromeWindow(GetNativeWindow(),
                                                            gfx::Rect(),
                                                            view);
  window->SetIsAlwaysOnTop(true);
  window->Show();
}

void ExistingUserController::OnHelpLinkActivated() {
  DCHECK(last_login_error_.state() != GoogleServiceAuthError::NONE);
  if (!help_app_.get())
    help_app_.reset(new HelpAppLauncher(GetNativeWindow()));
  switch (last_login_error_.state()) {
    case(GoogleServiceAuthError::CONNECTION_FAILED):
      help_app_->ShowHelpTopic(
          HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT_OFFLINE);
      break;
    case(GoogleServiceAuthError::ACCOUNT_DISABLED):
        help_app_->ShowHelpTopic(
            HelpAppLauncher::HELP_ACCOUNT_DISABLED);
        break;
    default:
      help_app_->ShowHelpTopic(login_timed_out_ ?
          HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT_OFFLINE :
          HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);
      break;
  }
}

void ExistingUserController::OnCaptchaEntered(const std::string& captcha) {
  login_captcha_ = captcha;
}

void ExistingUserController::RecoverEncryptedData(
    const std::string& old_password) {
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(authenticator_.get(),
                        &Authenticator::RecoverEncryptedData,
                        old_password,
                        cached_credentials_));
  cached_credentials_ = GaiaAuthConsumer::ClientLoginResult();
}

void ExistingUserController::ResyncEncryptedData() {
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(authenticator_.get(),
                        &Authenticator::ResyncEncryptedData,
                        cached_credentials_));
  cached_credentials_ = GaiaAuthConsumer::ClientLoginResult();
}

}  // namespace chromeos
