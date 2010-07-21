// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/existing_user_controller.h"

#include <algorithm>
#include <functional>
#include <map>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
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
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/message_bubble.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/profile_manager.h"
#include "gfx/native_widget_types.h"
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

// ClientLogin response parameters.
const char kError[] = "Error=";
const char kCaptchaError[] = "CaptchaRequired";
const char kCaptchaUrlParam[] = "CaptchaUrl=";
const char kCaptchaTokenParam[] = "CaptchaToken=";
const char kParamSuffix[] = "\n";

// URL prefix for CAPTCHA image.
const char kCaptchaUrlPrefix[] = "http://www.google.com/accounts/";

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
      bubble_(NULL) {
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
    background_window_->Show();
  }
  for (size_t i = 0; i < controllers_.size(); ++i) {
    (controllers_[i])->Init(static_cast<int>(i),
                            static_cast<int>(controllers_.size()));
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
  selected_view_index_ = i - controllers_.begin();

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
    controllers_[selected_view_index_]->ClearAndEnablePassword();
    ClearCaptchaState();
  }
  selected_view_index_ = new_selected_index;
}

void ExistingUserController::ActivateWizard(const std::string& screen_name) {
  // WizardController takes care of deleting itself when done.
  WizardController* controller = new WizardController();
  controller->Init(screen_name, background_bounds_);
  controller->Show();

  // Give the background window to the controller.
  controller->OwnBackground(background_window_, background_view_);
  background_window_ = NULL;

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

void ExistingUserController::OnLoginFailure(const std::string& error) {
  LOG(INFO) << "OnLoginFailure";
  ClearCaptchaState();

  // Check networking after trying to login in case user is
  // cached locally or the local admin account.
  NetworkLibrary* network = CrosLibrary::Get()->GetNetworkLibrary();
  if (!network || !CrosLibrary::Get()->EnsureLoaded()) {
    ShowError(IDS_LOGIN_ERROR_NO_NETWORK_LIBRARY, error);
  } else if (!network->Connected()) {
    ShowError(IDS_LOGIN_ERROR_OFFLINE_FAILED_NETWORK_NOT_CONNECTED, error);
  } else {
    std::string error_code = LoginUtils::ExtractClientLoginParam(error,
                                                                 kError,
                                                                 kParamSuffix);
    std::string captcha_url;
    if (error_code == kCaptchaError)
      captcha_url = LoginUtils::ExtractClientLoginParam(error,
                                                        kCaptchaUrlParam,
                                                        kParamSuffix);
    if (captcha_url.empty()) {
      ShowError(IDS_LOGIN_ERROR_AUTHENTICATING, error);
    } else {
      // Save token for next login retry.
      login_token_ = LoginUtils::ExtractClientLoginParam(error,
                                                         kCaptchaTokenParam,
                                                         kParamSuffix);
      CaptchaView* view =
          new CaptchaView(GURL(kCaptchaUrlPrefix + captcha_url));
      view->set_delegate(this);
      views::Window* window = views::Window::CreateChromeWindow(
          GetNativeWindow(), gfx::Rect(), view);
      window->SetIsAlwaysOnTop(true);
      window->Show();
    }
  }

  controllers_[selected_view_index_]->ClearAndEnablePassword();

  // Reenable clicking on other windows.
  SendSetLoginState(true);
}

void ExistingUserController::AppendStartUrlToCmdline() {
  if (start_url_.is_valid()) {
    CommandLine::ForCurrentProcess()->AppendLooseValue(
        UTF8ToWide(start_url_.spec()));
  }
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
    const GaiaAuthConsumer::ClientLoginResult& credentials) {

  AppendStartUrlToCmdline();
  if (selected_view_index_ + 1 == controllers_.size()) {
    // For new user login don't launch browser until we pass image screen.
    chromeos::LoginUtils::Get()->EnableBrowserLaunch(false);
    LoginUtils::Get()->CompleteLogin(username, credentials);
    ActivateWizard(WizardController::kUserImageScreenName);
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
  AppendStartUrlToCmdline();
  LoginUtils::Get()->CompleteOffTheRecordLogin();
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
