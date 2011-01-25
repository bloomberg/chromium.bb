// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/existing_user_controller.h"

#include <algorithm>
#include <functional>
#include <map>

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/background_view.h"
#include "chrome/browser/chromeos/login/help_app_launcher.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/message_bubble.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/status/status_area_view.h"
#include "chrome/browser/chromeos/user_cros_settings_provider.h"
#include "chrome/browser/chromeos/view_ids.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "gfx/native_widget_types.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/screen.h"
#include "views/widget/widget_gtk.h"
#include "views/window/window.h"

namespace chromeos {

namespace {

// Max number of users we'll show. The true max is the min of this and the
// number of windows that fit on the screen.
const size_t kMaxUsers = 6;

// Minimum number of users we'll show (including Guest and New User pods).
const size_t kMinUsers = 3;

// Used to indicate no user has been selected.
const size_t kNotSelected = -1;

// Offset of cursor in first position from edit left side. It's used to position
// info bubble arrow to cursor.
const int kCursorOffset = 5;

// Url for setting up sync authentication.
const char kSettingsSyncLoginUrl[] = "chrome://settings/personal";

// URL that will be opened on when user logs in first time on the device.
const char kGetStartedURL[] =
    "chrome-extension://nbaambmfhicobichobkkokacjbaoinda/index.html";

// Path to extracted version of Get Started app.
const char kGetStartedPath[] = "/usr/share/chromeos-assets/getstarted";

// Used to handle the asynchronous response of deleting a cryptohome directory.
class RemoveAttempt : public CryptohomeLibrary::Delegate {
 public:
  explicit RemoveAttempt(const std::string& user_email)
      : user_email_(user_email) {
    if (CrosLibrary::Get()->EnsureLoaded()) {
      CrosLibrary::Get()->GetCryptohomeLibrary()->AsyncRemove(
          user_email_, this);
    }
  }

  void OnComplete(bool success, int return_code) {
    // Log the error, but there's not much we can do.
    if (!success) {
      VLOG(1) << "Removal of cryptohome for " << user_email_
              << " failed, return code: " << return_code;
    }
    delete this;
  }

 private:
  std::string user_email_;
};

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
  for (size_t i = 0; i < controllers.size(); ++i) {
    const std::string& display_name =
        controllers[i]->user().GetDisplayName();
    bool show_tooltip = controllers[i]->is_new_user() ||
                        controllers[i]->is_guest() ||
                        visible_display_names[display_name] > 1;
    controllers[i]->EnableNameTooltip(show_tooltip);
  }
}

}  // namespace

ExistingUserController*
  ExistingUserController::current_controller_ = NULL;

// TODO(xiyuan): Wait for the cached settings update before using them.
ExistingUserController::ExistingUserController(
    const std::vector<UserManager::User>& users,
    const gfx::Rect& background_bounds)
    : background_bounds_(background_bounds),
      background_window_(NULL),
      background_view_(NULL),
      selected_view_index_(kNotSelected),
      num_login_attempts_(0),
      bubble_(NULL),
      user_settings_(new UserCrosSettingsProvider),
      method_factory_(this) {
  if (current_controller_)
    current_controller_->Delete();
  current_controller_ = this;

  // Calculate the max number of users from available screen size.
  bool show_guest = UserCrosSettingsProvider::cached_allow_guest();
  bool show_new_user = true;
  if (UserCrosSettingsProvider::cached_show_users_on_signin()) {
    size_t max_users = kMaxUsers;
    int screen_width = background_bounds.width();
    if (screen_width > 0) {
      size_t users_per_screen = (screen_width - login::kUserImageSize)
          / (UserController::kUnselectedSize + UserController::kPadding);
      max_users = std::max(kMinUsers, std::min(kMaxUsers, users_per_screen));
    }

    size_t visible_users_count = std::min(users.size(), max_users -
        static_cast<int>(show_guest) - static_cast<int>(show_new_user));
    for (size_t i = 0; i < users.size(); ++i) {
      // TODO(xiyuan): Clean user profile whose email is not in whitelist.
      if (UserCrosSettingsProvider::cached_allow_new_user() ||
          UserCrosSettingsProvider::IsEmailInCachedWhitelist(
              users[i].email())) {
        UserController* user_controller = new UserController(this, users[i]);
        if (controllers_.size() < visible_users_count)
          controllers_.push_back(user_controller);
        else
          invisible_controllers_.push_back(user_controller);
      }
    }
  }

  if (!controllers_.empty() && show_guest)
    controllers_.push_back(new UserController(this, true));

  if (show_new_user)
    controllers_.push_back(new UserController(this, false));
}

void ExistingUserController::Init() {
  if (!background_window_) {
    std::string url_string =
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kScreenSaverUrl);

    background_window_ = BackgroundView::CreateWindowContainingView(
        background_bounds_,
        GURL(url_string),
        &background_view_);
    background_view_->EnableShutdownButton(true);

    if (!WizardController::IsDeviceRegistered()) {
      background_view_->SetOobeProgressBarVisible(true);
      background_view_->SetOobeProgress(chromeos::BackgroundView::SIGNIN);
    }

    background_window_->Show();
  }
  // If there's only new user pod, show the guest session link on it.
  bool show_guest_link = controllers_.size() == 1;
  for (size_t i = 0; i < controllers_.size(); ++i) {
    (controllers_[i])->Init(static_cast<int>(i),
                            static_cast<int>(controllers_.size()),
                            show_guest_link);
  }

  EnableTooltipsIfNeeded(controllers_);

  WmMessageListener::GetInstance()->AddObserver(this);

  LoginUtils::Get()->PrewarmAuthentication();
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

void ExistingUserController::LoginNewUser(const std::string& username,
                                          const std::string& password) {
  SelectNewUser();
  UserController* new_user = controllers_.back();
  DCHECK(new_user->is_new_user());
  if (!new_user->is_new_user())
    return;

  if (password.empty())
    return;

  new_user->OnLogin(username, password);
}

void ExistingUserController::SelectNewUser() {
  SelectUser(controllers_.size() - 1);
}

void ExistingUserController::SetStatusAreaEnabled(bool enable) {
  if (background_view_) {
    background_view_->SetStatusAreaEnabled(enable);
  }
}

ExistingUserController::~ExistingUserController() {
  ClearErrors();

  if (background_window_)
    background_window_->Close();

  WmMessageListener::GetInstance()->RemoveObserver(this);

  STLDeleteElements(&controllers_);
  STLDeleteElements(&invisible_controllers_);
  DCHECK(current_controller_ != NULL);
  current_controller_ = NULL;
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

  // Disable clicking on other windows.
  SendSetLoginState(false);

  // Use the same LoginPerformer for subsequent login as it has state
  // such as CAPTCHA challenge token & corresponding user input.
  if (!login_performer_.get() || num_login_attempts_ <= 1) {
    LoginPerformer::Delegate* delegate = this;
    if (login_performer_delegate_.get())
      delegate = login_performer_delegate_.get();
    login_performer_.reset(new LoginPerformer(delegate));
  }
  login_performer_->Login(controllers_[selected_view_index_]->user().email(),
                          UTF16ToUTF8(password));
}

void ExistingUserController::WhiteListCheckFailed(const std::string& email) {
  ShowError(IDS_LOGIN_ERROR_WHITELIST, email);

  // Reenable userview and use ClearAndEnablePassword to keep username on
  // screen with the error bubble.
  controllers_[selected_view_index_]->ClearAndEnablePassword();

  // Reenable clicking on other windows.
  SendSetLoginState(true);
}

void ExistingUserController::LoginOffTheRecord() {
  // Check allow_guest in case this call is fired from key accelerator.
  // Must not proceed without signature verification.
  bool trusted_setting_available = user_settings_->RequestTrustedAllowGuest(
      method_factory_.NewRunnableMethod(
          &ExistingUserController::LoginOffTheRecord));
  if (!trusted_setting_available) {
    // Value of AllowGuest setting is still not verified.
    // Another attempt will be invoked again after verification completion.
    return;
  }
  if (!UserCrosSettingsProvider::cached_allow_guest()) {
    // Disallowed.
    return;
  }

  // Disable clicking on other windows.
  SendSetLoginState(false);

  login_performer_.reset(new LoginPerformer(this));
  login_performer_->LoginOffTheRecord();
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
    controllers_[new_selected_index]->ClearAndEnableFields();
    login_performer_.reset(NULL);
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

  delete_timer_.Start(base::TimeDelta::FromSeconds(1), this,
                      &ExistingUserController::Delete);
}

void ExistingUserController::RemoveUser(UserController* source) {
  ClearErrors();

  // Owner is not allowed to be removed from the device.
  // It must be enforced at upper levels.
  DCHECK(user_settings_->RequestTrustedOwner(NULL));
  DCHECK(source->user().email() != UserCrosSettingsProvider::cached_owner());

  UserManager::Get()->RemoveUser(source->user().email());

  controllers_.erase(controllers_.begin() + source->user_index());

  EnableTooltipsIfNeeded(controllers_);

  // Update user count before unmapping windows, otherwise window manager won't
  // be in the right state.
  int new_size = static_cast<int>(controllers_.size());
  for (int i = 0; i < new_size; ++i)
    controllers_[i]->UpdateUserCount(i, new_size);

  // Delete the encrypted user directory.
  new RemoveAttempt(source->user().email());
  // We need to unmap entry windows, the windows will be unmapped in destructor.
  delete source;

  // Nothing to insert.
  if (invisible_controllers_.empty())
    return;

  // Insert just before guest or add new user pods if any.
  int insert_position = new_size;
  while (insert_position > 0 &&
         (controllers_[insert_position - 1]->is_new_user() ||
          controllers_[insert_position - 1]->is_guest()))
    --insert_position;

  controllers_.insert(controllers_.begin() + insert_position,
                      invisible_controllers_[0]);
  invisible_controllers_.erase(invisible_controllers_.begin());

  // Update counts for exiting pods.
  new_size = static_cast<int>(controllers_.size());
  for (int i = 0; i < new_size; ++i) {
    if (i != insert_position)
      controllers_[i]->UpdateUserCount(i, new_size);
  }

  // And initialize new one that was invisible.
  controllers_[insert_position]->Init(insert_position, new_size, false);

  EnableTooltipsIfNeeded(controllers_);
}

void ExistingUserController::SelectUser(int index) {
  if (index >= 0 && index < static_cast<int>(controllers_.size()) &&
      index != static_cast<int>(selected_view_index_)) {
    WmIpc::Message message(WM_IPC_MESSAGE_WM_SELECT_LOGIN_USER);
    message.set_param(0, index);
    WmIpc::instance()->SendMessage(message);
  }
}

void ExistingUserController::OnLoginFailure(const LoginFailure& failure) {
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
        CaptchaView* view =
            new CaptchaView(failure.error().captcha().image_url, false);
        view->Init();
        view->set_delegate(this);
        views::Window* window = browser::CreateViewsWindow(
            GetNativeWindow(), gfx::Rect(), view);
        window->SetIsAlwaysOnTop(true);
        window->Show();
      } else {
        LOG(WARNING) << "No captcha image url was found?";
        ShowError(IDS_LOGIN_ERROR_AUTHENTICATING, error);
      }
    } else if (failure.reason() == LoginFailure::NETWORK_AUTH_FAILED &&
               failure.error().state() ==
                   GoogleServiceAuthError::HOSTED_NOT_ALLOWED) {
      ShowError(IDS_LOGIN_ERROR_AUTHENTICATING_HOSTED, error);
    } else {
      if (controllers_[selected_view_index_]->is_new_user())
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

gfx::NativeWindow ExistingUserController::GetNativeWindow() const {
  return GTK_WINDOW(
      static_cast<views::WidgetGtk*>(background_window_)->GetNativeView());
}

void ExistingUserController::ShowError(int error_id,
                                       const std::string& details) {
  ClearErrors();
  string16 error_text;
  // GetStringF fails on debug build if there's no replacement in the string.
  if (error_id == IDS_LOGIN_ERROR_AUTHENTICATING_HOSTED) {
    error_text = l10n_util::GetStringFUTF16(
        error_id, l10n_util::GetStringUTF16(IDS_PRODUCT_OS_NAME));
  } else {
    error_text = l10n_util::GetStringUTF16(error_id);
  }
  // TODO(dpolukhin): show detailed error info. |details| string contains
  // low level error info that is not localized and even is not user friendly.
  // For now just ignore it because error_text contains all required information
  // for end users, developers can see details string in Chrome logs.

  gfx::Rect bounds =
      controllers_[selected_view_index_]->GetMainInputScreenBounds();
  BubbleBorder::ArrowLocation arrow;
  if (controllers_[selected_view_index_]->is_new_user()) {
    arrow = BubbleBorder::LEFT_TOP;
  } else {
    // Point info bubble arrow to cursor position (approximately).
    bounds.set_width(kCursorOffset * 2);
    arrow = BubbleBorder::BOTTOM_LEFT;
  }
  string16 help_link;
  if (error_id == IDS_LOGIN_ERROR_AUTHENTICATING_HOSTED) {
    help_link = l10n_util::GetStringUTF16(IDS_LEARN_MORE);
  } else if (num_login_attempts_ > static_cast<size_t>(1)) {
    help_link = l10n_util::GetStringUTF16(IDS_CANT_ACCESS_ACCOUNT_BUTTON);
  }

  bubble_ = MessageBubble::Show(
      controllers_[selected_view_index_]->controls_window(),
      bounds,
      arrow,
      ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_WARNING),
      UTF16ToWide(error_text),
      UTF16ToWide(help_link),
      this);
}

void ExistingUserController::OnLoginSuccess(
    const std::string& username,
    const std::string& password,
    const GaiaAuthConsumer::ClientLoginResult& credentials,
    bool pending_requests) {
  // LoginPerformer instance will delete itself once online auth result is OK.
  // In case of failure it'll bring up ScreenLock and ask for
  // correct password/display error message.
  // Even in case when following online,offline protocol and returning
  // requests_pending = false, let LoginPerformer delete itself.
  login_performer_->set_delegate(NULL);
  LoginPerformer* performer = login_performer_.release();
  performer = NULL;
  bool known_user = UserManager::Get()->IsKnownUser(username);
  bool login_only =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kLoginScreen) == WizardController::kLoginScreenName;
  AppendStartUrlToCmdline();
  controllers_[selected_view_index_]->StopThrobber();
  if (selected_view_index_ + 1 == controllers_.size() &&
      !known_user && !login_only) {
#if defined(OFFICIAL_BUILD)
    CommandLine::ForCurrentProcess()->AppendSwitchPath(
        switches::kLoadExtension,
        FilePath(kGetStartedPath));
    CommandLine::ForCurrentProcess()->AppendArg(kGetStartedURL);
#endif  // OFFICIAL_BUILD
    if (credentials.two_factor) {
      // If we have a two factor error and and this is a new user,
      // load the personal settings page.
      // TODO(stevenjb): direct the user to a lightweight sync login page.
      CommandLine::ForCurrentProcess()->AppendArg(kSettingsSyncLoginUrl);
    }
    // For new user login don't launch browser until we pass image screen.
    LoginUtils::Get()->EnableBrowserLaunch(false);
    LoginUtils::Get()->CompleteLogin(username,
                                     password,
                                     credentials,
                                     pending_requests);
    ActivateWizard(WizardController::IsDeviceRegistered() ?
        WizardController::kUserImageScreenName :
        WizardController::kRegistrationScreenName);
  } else {
    // Hide the login windows now.
    WmIpc::Message message(WM_IPC_MESSAGE_WM_HIDE_LOGIN);
    WmIpc::instance()->SendMessage(message);

    LoginUtils::Get()->CompleteLogin(username,
                                     password,
                                     credentials,
                                     pending_requests);

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
  // Must not proceed without signature verification.
  bool trusted_setting_available = user_settings_->RequestTrustedOwner(
      method_factory_.NewRunnableMethod(
          &ExistingUserController::OnPasswordChangeDetected,
          credentials));
  if (!trusted_setting_available) {
    // Value of owner email is still not verified.
    // Another attempt will be invoked after verification completion.
    return;
  }
  // TODO(altimofeev): remove this constrain when full sync for the owner will
  // be correctly handled.
  bool full_sync_disabled = (UserCrosSettingsProvider::cached_owner() ==
      controllers_[selected_view_index_]->user().email());

  PasswordChangedView* view = new PasswordChangedView(this, full_sync_disabled);
  views::Window* window = browser::CreateViewsWindow(GetNativeWindow(),
                                                     gfx::Rect(),
                                                     view);
  window->SetIsAlwaysOnTop(true);
  window->Show();
}

void ExistingUserController::OnHelpLinkActivated() {
  DCHECK(login_performer_->error().state() != GoogleServiceAuthError::NONE);
  if (!help_app_.get())
    help_app_.reset(new HelpAppLauncher(GetNativeWindow()));
  switch (login_performer_->error().state()) {
    case GoogleServiceAuthError::CONNECTION_FAILED:
      help_app_->ShowHelpTopic(
          HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT_OFFLINE);
      break;
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
        help_app_->ShowHelpTopic(
            HelpAppLauncher::HELP_ACCOUNT_DISABLED);
        break;
    case GoogleServiceAuthError::HOSTED_NOT_ALLOWED:
        help_app_->ShowHelpTopic(
            HelpAppLauncher::HELP_HOSTED_ACCOUNT);
        break;
    default:
      help_app_->ShowHelpTopic(login_performer_->login_timed_out() ?
          HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT_OFFLINE :
          HelpAppLauncher::HELP_CANT_ACCESS_ACCOUNT);
      break;
  }
}

void ExistingUserController::OnCaptchaEntered(const std::string& captcha) {
  login_performer_->set_captcha(captcha);
}

void ExistingUserController::RecoverEncryptedData(
    const std::string& old_password) {
  // LoginPerformer instance has state of the user so it should exist.
  if (login_performer_.get())
    login_performer_->RecoverEncryptedData(old_password);
}

void ExistingUserController::ResyncEncryptedData() {
  // LoginPerformer instance has state of the user so it should exist.
  if (login_performer_.get())
    login_performer_->ResyncEncryptedData();
}

}  // namespace chromeos
