// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_display_host_mojo.h"

#include <string>
#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/mojo_version_info_dispatcher.h"
#include "chrome/browser/chromeos/login/screens/chrome_user_selection_screen.h"
#include "chrome/browser/chromeos/login/screens/gaia_view.h"
#include "chrome/browser/chromeos/login/ui/login_display.h"
#include "chrome/browser/chromeos/login/ui/login_display_mojo.h"
#include "chrome/browser/chromeos/login/ui/oobe_ui_dialog_delegate.h"
#include "chrome/browser/chromeos/login/user_board_view_mojo.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chromeos/login/auth/user_context.h"
#include "components/user_manager/user_names.h"

namespace chromeos {

namespace {

constexpr char kLoginDisplay[] = "login";
constexpr char kAccelSendFeedback[] = "send_feedback";

}  // namespace

LoginDisplayHostMojo::AuthState::AuthState(AccountId account_id,
                                           AuthenticateUserCallback callback)
    : account_id(account_id), callback(std::move(callback)) {}

LoginDisplayHostMojo::AuthState::~AuthState() = default;

LoginDisplayHostMojo::LoginDisplayHostMojo()
    : login_display_(std::make_unique<LoginDisplayMojo>(this)),
      user_board_view_mojo_(std::make_unique<UserBoardViewMojo>()),
      user_selection_screen_(
          std::make_unique<ChromeUserSelectionScreen>(kLoginDisplay)),
      version_info_updater_(std::make_unique<MojoVersionInfoDispatcher>()),
      weak_factory_(this) {
  user_selection_screen_->SetView(user_board_view_mojo_.get());

  // Preload webui-based OOBE for add user, kiosk apps, etc.
  LoadOobeDialog();
}

LoginDisplayHostMojo::~LoginDisplayHostMojo() {
  LoginScreenClient::Get()->SetDelegate(nullptr);
  if (dialog_) {
    dialog_->GetOobeUI()->signin_screen_handler()->SetDelegate(nullptr);
    dialog_->Close();
  }
}

void LoginDisplayHostMojo::OnDialogDestroyed(
    const OobeUIDialogDelegate* dialog) {
  if (dialog == dialog_) {
    dialog_ = nullptr;
    wizard_controller_.reset();
  }
}

void LoginDisplayHostMojo::SetUsers(const user_manager::UserList& users) {
  users_ = users;
  if (GetOobeUI())
    GetOobeUI()->SetLoginUserCount(users_.size());
}

void LoginDisplayHostMojo::ShowPasswordChangedDialog(bool show_password_error,
                                                     const std::string& email) {
  DCHECK(GetOobeUI());
  GetOobeUI()->signin_screen_handler()->ShowPasswordChangedDialog(
      show_password_error, email);
  dialog_->Show(false /*closable_by_esc*/);
}

void LoginDisplayHostMojo::ShowWhitelistCheckFailedError() {
  DCHECK(GetOobeUI());
  GetOobeUI()->signin_screen_handler()->ShowWhitelistCheckFailedError();
  dialog_->Show(true /*closable_by_esc*/);
}

void LoginDisplayHostMojo::ShowUnrecoverableCrypthomeErrorDialog() {
  DCHECK(GetOobeUI());
  GetOobeUI()->signin_screen_handler()->ShowUnrecoverableCrypthomeErrorDialog();
  dialog_->Show(false /*closable_by_esc*/);
}

void LoginDisplayHostMojo::ShowErrorScreen(LoginDisplay::SigninError error_id) {
  DCHECK(GetOobeUI());
  GetOobeUI()->signin_screen_handler()->ShowErrorScreen(error_id);
  dialog_->Show(false /*closable_by_esc*/);
}

void LoginDisplayHostMojo::ShowSigninUI(const std::string& email) {
  DCHECK(GetOobeUI());
  GetOobeUI()->signin_screen_handler()->ShowSigninUI(email);
  dialog_->Show(true /*closable_by_esc*/);
}

LoginDisplay* LoginDisplayHostMojo::GetLoginDisplay() {
  return login_display_.get();
}

ExistingUserController* LoginDisplayHostMojo::GetExistingUserController() {
  return existing_user_controller_.get();
}

gfx::NativeWindow LoginDisplayHostMojo::GetNativeWindow() const {
  // We can't access the login widget because it's in ash, return the native
  // window of the dialog widget if it exists.
  if (!dialog_)
    return nullptr;
  return dialog_->GetNativeWindow();
}

OobeUI* LoginDisplayHostMojo::GetOobeUI() const {
  if (!dialog_)
    return nullptr;
  return dialog_->GetOobeUI();
}

content::WebContents* LoginDisplayHostMojo::GetOobeWebContents() const {
  if (!dialog_)
    return nullptr;
  return dialog_->GetWebContents();
}

WebUILoginView* LoginDisplayHostMojo::GetWebUILoginView() const {
  NOTREACHED();
  return nullptr;
}

void LoginDisplayHostMojo::OnFinalize() {
  if (dialog_)
    dialog_->Close();

  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void LoginDisplayHostMojo::SetStatusAreaVisible(bool visible) {
  NOTIMPLEMENTED();
}

void LoginDisplayHostMojo::StartWizard(OobeScreen first_screen) {
  DCHECK(GetOobeUI());

  wizard_controller_ = std::make_unique<WizardController>();
  wizard_controller_->Init(first_screen);

  // Post login screens should not be closable by escape key.
  dialog_->Show(false /*closable_by_esc*/);
}

WizardController* LoginDisplayHostMojo::GetWizardController() {
  return wizard_controller_.get();
}

void LoginDisplayHostMojo::OnStartUserAdding() {
  NOTIMPLEMENTED();
}

void LoginDisplayHostMojo::CancelUserAdding() {
  NOTIMPLEMENTED();
}

void LoginDisplayHostMojo::OnStartSignInScreen(
    const LoginScreenContext& context) {
  // This function may be called early in startup flow, before LoginScreenClient
  // has been initialized. Wait until LoginScreenClient is initialized as it is
  // a common dependency.
  if (!LoginScreenClient::HasInstance()) {
    // TODO(jdufault): Add a timeout here / make sure we do not post infinitely.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&LoginDisplayHostMojo::OnStartSignInScreen,
                                  weak_factory_.GetWeakPtr(), context));
    return;
  }

  existing_user_controller_ = std::make_unique<ExistingUserController>();
  login_display_->set_delegate(existing_user_controller_.get());

  // We need auth attempt results to notify views-based lock screen.
  existing_user_controller_->set_login_status_consumer(this);

  // Load the UI.
  existing_user_controller_->Init(user_manager::UserManager::Get()->GetUsers());

  user_selection_screen_->InitEasyUnlock();

  kiosk_updater_.SendKioskApps();

  // Start to request version info.
  version_info_updater_->StartUpdate();
}

void LoginDisplayHostMojo::OnPreferencesChanged() {
  NOTIMPLEMENTED();
}

void LoginDisplayHostMojo::OnStartAppLaunch() {
  dialog_->ShowFullScreen();
}

void LoginDisplayHostMojo::OnStartArcKiosk() {
  dialog_->ShowFullScreen();
}

void LoginDisplayHostMojo::OnBrowserCreated() {
  NOTIMPLEMENTED();
}

void LoginDisplayHostMojo::StartVoiceInteractionOobe() {
  NOTIMPLEMENTED();
}

bool LoginDisplayHostMojo::IsVoiceInteractionOobe() {
  NOTIMPLEMENTED();
  return false;
}

void LoginDisplayHostMojo::ShowGaiaDialog(
    bool can_close,
    const base::Optional<AccountId>& prefilled_account) {
  DCHECK(GetOobeUI());
  can_close_dialog_ = can_close;

  // Always disabling closing if there are no users, otherwise a blank screen
  // will be displayed.
  if (users_.empty())
    can_close_dialog_ = false;

  if (prefilled_account) {
    // Make sure gaia displays |account| if requested.
    if (!login_display_->IsSigninInProgress())
      GetOobeUI()->GetGaiaScreenView()->ShowGaiaAsync(prefilled_account);
    LoadWallpaper(*prefilled_account);
  } else {
    LoadSigninWallpaper();
  }

  dialog_->Show(can_close /*closable_by_esc*/);
  return;
}

void LoginDisplayHostMojo::HideOobeDialog() {
  DCHECK(dialog_);
  if (!can_close_dialog_)
    return;

  // The dialog can not be hidden if there are no users on the login screen.
  // Reload it instead.
  if (!login_display_->IsSigninInProgress() && users_.empty()) {
    GetOobeUI()->GetGaiaScreenView()->ShowGaiaAsync(base::nullopt);
    return;
  }

  LoadWallpaper(focused_pod_account_id_);
  dialog_->Hide();
}

void LoginDisplayHostMojo::UpdateOobeDialogSize(int width, int height) {
  if (dialog_)
    dialog_->UpdateSizeAndPosition(width, height);
}

const user_manager::UserList LoginDisplayHostMojo::GetUsers() {
  return users_;
}

void LoginDisplayHostMojo::ShowFeedback() {
  DCHECK(GetOobeUI());
  GetOobeUI()->ForwardAccelerator(kAccelSendFeedback);
}

void LoginDisplayHostMojo::OnCancelPasswordChangedFlow() {
  HideOobeDialog();
}

void LoginDisplayHostMojo::HandleAuthenticateUser(
    const AccountId& account_id,
    const std::string& password,
    bool authenticated_by_pin,
    AuthenticateUserCallback callback) {
  DCHECK_EQ(account_id.GetUserEmail(),
            gaia::SanitizeEmail(account_id.GetUserEmail()));

  CHECK(!pending_auth_state_);
  pending_auth_state_ =
      std::make_unique<AuthState>(account_id, std::move(callback));

  const user_manager::User* const user =
      user_manager::UserManager::Get()->FindUser(account_id);
  DCHECK(user);
  UserContext user_context(*user);
  user_context.SetIsUsingPin(authenticated_by_pin);
  user_context.SetKey(
      Key(chromeos::Key::KEY_TYPE_PASSWORD_PLAIN, "" /*salt*/, password));
  if (account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY &&
      (user_context.GetUserType() !=
       user_manager::UserType::USER_TYPE_ACTIVE_DIRECTORY)) {
    LOG(FATAL) << "Incorrect Active Directory user type "
               << user_context.GetUserType();
  }

  existing_user_controller_->Login(user_context, chromeos::SigninSpecifics());
}

void LoginDisplayHostMojo::HandleAttemptUnlock(const AccountId& account_id) {
  user_selection_screen_->AttemptEasyUnlock(account_id);
}

void LoginDisplayHostMojo::HandleHardlockPod(const AccountId& account_id) {
  user_selection_screen_->HardLockPod(account_id);
}

void LoginDisplayHostMojo::HandleRecordClickOnLockIcon(
    const AccountId& account_id) {
  user_selection_screen_->RecordClickOnLockIcon(account_id);
}

void LoginDisplayHostMojo::HandleOnFocusPod(const AccountId& account_id) {
  // TODO(jdufault): Share common code between this and
  // ViewsScreenLocker::HandleOnFocusPod See https://crbug.com/831787.
  proximity_auth::ScreenlockBridge::Get()->SetFocusedUser(account_id);
  user_selection_screen_->CheckUserStatus(account_id);
  WallpaperControllerClient::Get()->ShowUserWallpaper(account_id);
  focused_pod_account_id_ = account_id;
}

void LoginDisplayHostMojo::HandleOnNoPodFocused() {
  NOTIMPLEMENTED();
}

bool LoginDisplayHostMojo::HandleFocusLockScreenApps(bool reverse) {
  NOTREACHED();
  return false;
}

void LoginDisplayHostMojo::HandleLoginAsGuest() {
  existing_user_controller_->Login(UserContext(user_manager::USER_TYPE_GUEST,
                                               user_manager::GuestAccountId()),
                                   chromeos::SigninSpecifics());
}

void LoginDisplayHostMojo::HandleLaunchPublicSession(
    const AccountId& account_id,
    const std::string& locale,
    const std::string& input_method) {
  UserContext context(user_manager::USER_TYPE_PUBLIC_ACCOUNT, account_id);
  context.SetPublicSessionLocale(locale);
  context.SetPublicSessionInputMethod(input_method);
  existing_user_controller_->Login(context, chromeos::SigninSpecifics());
}

void LoginDisplayHostMojo::OnAuthFailure(const AuthFailure& error) {
  // OnAuthFailure and OnAuthSuccess can be called if an authentication attempt
  // is not initiated from mojo, ie, if LoginDisplay::Delegate::Login() is
  // called directly.
  if (pending_auth_state_) {
    login_display_->UpdatePinKeyboardState(pending_auth_state_->account_id);
    std::move(pending_auth_state_->callback).Run(false);
    pending_auth_state_.reset();
  }
}

void LoginDisplayHostMojo::OnAuthSuccess(const UserContext& user_context) {
  if (pending_auth_state_) {
    std::move(pending_auth_state_->callback).Run(true);
    pending_auth_state_.reset();
  }
}

void LoginDisplayHostMojo::LoadOobeDialog() {
  if (dialog_)
    return;

  dialog_ = new OobeUIDialogDelegate(weak_factory_.GetWeakPtr());
  dialog_->GetOobeUI()->signin_screen_handler()->SetDelegate(
      login_display_.get());
}

}  // namespace chromeos
