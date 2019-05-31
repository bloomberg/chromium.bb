// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/login_screen_controller.h"

#include <utility>

#include "ash/focus_cycler.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/login/ui/parent_access_widget.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/login_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/toast/toast_data.h"
#include "ash/system/toast/toast_manager.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/bind.h"
#include "base/debug/alias.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/session_manager/session_manager_types.h"

namespace ash {

namespace {

enum class SystemTrayVisibility {
  kNone,     // Tray not visible anywhere.
  kPrimary,  // Tray visible only on primary display.
  kAll,      // Tray visible on all displays.
};

void SetSystemTrayVisibility(SystemTrayVisibility visibility) {
  RootWindowController* primary_window_controller =
      Shell::GetPrimaryRootWindowController();
  for (RootWindowController* window_controller :
       Shell::GetAllRootWindowControllers()) {
    StatusAreaWidget* status_area = window_controller->GetStatusAreaWidget();
    if (!status_area)
      continue;
    if (window_controller == primary_window_controller) {
      status_area->SetSystemTrayVisibility(
          visibility == SystemTrayVisibility::kPrimary ||
          visibility == SystemTrayVisibility::kAll);
    } else {
      status_area->SetSystemTrayVisibility(visibility ==
                                           SystemTrayVisibility::kAll);
    }
  }
}

}  // namespace

LoginScreenController::LoginScreenController(
    SystemTrayNotifier* system_tray_notifier)
    : system_tray_notifier_(system_tray_notifier), weak_factory_(this) {
  system_tray_notifier_->AddSystemTrayFocusObserver(this);
}

LoginScreenController::~LoginScreenController() {
  system_tray_notifier_->RemoveSystemTrayFocusObserver(this);
}

// static
void LoginScreenController::RegisterProfilePrefs(PrefRegistrySimple* registry,
                                                 bool for_test) {
  if (for_test) {
    // There is no remote pref service, so pretend that ash owns the pref.
    registry->RegisterStringPref(prefs::kQuickUnlockPinSalt, "");
    return;
  }
}

void LoginScreenController::BindRequest(mojom::LoginScreenRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

bool LoginScreenController::IsAuthenticating() const {
  return authentication_stage_ != AuthenticationStage::kIdle;
}

void LoginScreenController::AuthenticateUserWithPasswordOrPin(
    const AccountId& account_id,
    const std::string& password,
    bool authenticated_by_pin,
    OnAuthenticateCallback callback) {
  // It is an error to call this function while an authentication is in
  // progress.
  LOG_IF(FATAL, IsAuthenticating())
      << "Duplicate authentication attempt; current authentication stage is "
      << static_cast<int>(authentication_stage_);

  if (!login_screen_client_) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  // If auth is disabled by the debug overlay bypass the mojo call entirely, as
  // it will dismiss the lock screen if the password is correct.
  switch (force_fail_auth_for_debug_overlay_) {
    case ForceFailAuth::kOff:
      break;
    case ForceFailAuth::kImmediate:
      OnAuthenticateComplete(std::move(callback), false /*success*/);
      return;
    case ForceFailAuth::kDelayed:
      // Set a dummy authentication stage so that |IsAuthenticating| returns
      // true.
      authentication_stage_ = AuthenticationStage::kDoAuthenticate;
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&LoginScreenController::OnAuthenticateComplete,
                         weak_factory_.GetWeakPtr(), base::Passed(&callback),
                         false),
          base::TimeDelta::FromSeconds(1));
      return;
  }

  authentication_stage_ = AuthenticationStage::kDoAuthenticate;

  int dummy_value;
  bool is_pin =
      authenticated_by_pin && base::StringToInt(password, &dummy_value);
  login_screen_client_->AuthenticateUserWithPasswordOrPin(
      account_id, password, is_pin,
      base::BindOnce(&LoginScreenController::OnAuthenticateComplete,
                     weak_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void LoginScreenController::AuthenticateUserWithExternalBinary(
    const AccountId& account_id,
    OnAuthenticateCallback callback) {
  // It is an error to call this function while an authentication is in
  // progress.
  LOG_IF(FATAL, IsAuthenticating())
      << "Duplicate authentication attempt; current authentication stage is "
      << static_cast<int>(authentication_stage_);

  if (!login_screen_client_) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  authentication_stage_ = AuthenticationStage::kDoAuthenticate;
  login_screen_client_->AuthenticateUserWithExternalBinary(
      account_id,
      base::BindOnce(&LoginScreenController::OnAuthenticateComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void LoginScreenController::EnrollUserWithExternalBinary(
    OnAuthenticateCallback callback) {
  if (!login_screen_client_) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  login_screen_client_->EnrollUserWithExternalBinary(base::BindOnce(
      [](OnAuthenticateCallback callback, bool success) {
        std::move(callback).Run(base::make_optional<bool>(success));
      },
      std::move(callback)));
}

void LoginScreenController::AuthenticateUserWithEasyUnlock(
    const AccountId& account_id) {
  // TODO(jdufault): integrate this into authenticate stage after mojom is
  // refactored to use a callback.
  if (!login_screen_client_)
    return;
  login_screen_client_->AuthenticateUserWithEasyUnlock(account_id);
}

void LoginScreenController::ValidateParentAccessCode(
    const AccountId& account_id,
    const std::string& code,
    OnParentAccessValidation callback) {
  if (!login_screen_client_) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  login_screen_client_->ValidateParentAccessCode(
      account_id, code,
      base::BindOnce(&LoginScreenController::OnParentAccessValidationComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void LoginScreenController::HardlockPod(const AccountId& account_id) {
  if (!login_screen_client_)
    return;
  login_screen_client_->HardlockPod(account_id);
}

void LoginScreenController::OnFocusPod(const AccountId& account_id) {
  if (!login_screen_client_)
    return;
  login_screen_client_->OnFocusPod(account_id);
}

void LoginScreenController::OnNoPodFocused() {
  if (!login_screen_client_)
    return;
  login_screen_client_->OnNoPodFocused();
}

void LoginScreenController::LoadWallpaper(const AccountId& account_id) {
  if (!login_screen_client_)
    return;
  login_screen_client_->LoadWallpaper(account_id);
}

void LoginScreenController::SignOutUser() {
  if (!login_screen_client_)
    return;
  login_screen_client_->SignOutUser();
}

void LoginScreenController::CancelAddUser() {
  if (!login_screen_client_)
    return;
  login_screen_client_->CancelAddUser();
}

void LoginScreenController::LoginAsGuest() {
  if (!login_screen_client_)
    return;
  login_screen_client_->LoginAsGuest();
}

void LoginScreenController::OnMaxIncorrectPasswordAttempted(
    const AccountId& account_id) {
  if (!login_screen_client_)
    return;
  login_screen_client_->OnMaxIncorrectPasswordAttempted(account_id);
}

void LoginScreenController::FocusLockScreenApps(bool reverse) {
  if (!login_screen_client_)
    return;
  login_screen_client_->FocusLockScreenApps(reverse);
}

void LoginScreenController::ShowGaiaSignin(
    bool can_close,
    const base::Optional<AccountId>& prefilled_account) {
  if (!login_screen_client_)
    return;
  login_screen_client_->ShowGaiaSignin(can_close, prefilled_account);
}

void LoginScreenController::OnRemoveUserWarningShown() {
  if (!login_screen_client_)
    return;
  login_screen_client_->OnRemoveUserWarningShown();
}

void LoginScreenController::RemoveUser(const AccountId& account_id) {
  if (!login_screen_client_)
    return;
  login_screen_client_->RemoveUser(account_id);
}

void LoginScreenController::LaunchPublicSession(
    const AccountId& account_id,
    const std::string& locale,
    const std::string& input_method) {
  if (!login_screen_client_)
    return;
  login_screen_client_->LaunchPublicSession(account_id, locale, input_method);
}

void LoginScreenController::RequestPublicSessionKeyboardLayouts(
    const AccountId& account_id,
    const std::string& locale) {
  if (!login_screen_client_)
    return;
  login_screen_client_->RequestPublicSessionKeyboardLayouts(account_id, locale);
}

void LoginScreenController::ShowFeedback() {
  if (!login_screen_client_)
    return;
  login_screen_client_->ShowFeedback();
}

void LoginScreenController::FlushForTesting() {
  login_screen_client_.FlushForTesting();
}

LoginScreenModel* LoginScreenController::GetModel() {
  return &login_data_dispatcher_;
}

void LoginScreenController::ShowParentAccessWidget(
    const AccountId& child_account_id,
    base::RepeatingCallback<void(bool success)> callback) {
  parent_access_widget_ =
      std::make_unique<ash::ParentAccessWidget>(child_account_id, callback);
}

void LoginScreenController::SetClient(mojom::LoginScreenClientPtr client) {
  login_screen_client_ = std::move(client);
}

void LoginScreenController::ShowLockScreen(ShowLockScreenCallback on_shown) {
  OnShow();
  ash::LockScreen::Show(ash::LockScreen::ScreenType::kLock);
  std::move(on_shown).Run(true);
}

void LoginScreenController::ShowLoginScreen(ShowLoginScreenCallback on_shown) {
  // Login screen can only be used during login.
  if (Shell::Get()->session_controller()->GetSessionState() !=
      session_manager::SessionState::LOGIN_PRIMARY) {
    LOG(ERROR) << "Not showing login screen since session state is "
               << static_cast<int>(
                      Shell::Get()->session_controller()->GetSessionState());
    std::move(on_shown).Run(false);
    return;
  }

  OnShow();
  // TODO(jdufault): rename ash::LockScreen to ash::LoginScreen.
  ash::LockScreen::Show(ash::LockScreen::ScreenType::kLogin);
  std::move(on_shown).Run(true);
}

void LoginScreenController::ShowErrorMessage(int32_t login_attempts,
                                             const std::string& error_text,
                                             const std::string& help_link_text,
                                             int32_t help_topic_id) {
  NOTIMPLEMENTED();
}

void LoginScreenController::ClearErrors() {
  NOTIMPLEMENTED();
}

void LoginScreenController::IsReadyForPassword(
    IsReadyForPasswordCallback callback) {
  std::move(callback).Run(LockScreen::HasInstance() && !IsAuthenticating());
}

void LoginScreenController::ShowKioskAppError(const std::string& message) {
  ToastData toast_data(
      "KioskAppError", base::UTF8ToUTF16(message), -1 /*duration_ms*/,
      base::Optional<base::string16>(base::string16()) /*dismiss_text*/,
      true /*visible_on_lock_screen*/);
  Shell::Get()->toast_manager()->Show(toast_data);
}

void LoginScreenController::SetAllowLoginAsGuest(bool allow_guest) {
  Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->login_shelf_view()
      ->SetAllowLoginAsGuest(allow_guest);
}

void LoginScreenController::SetShowGuestButtonInOobe(bool show) {
  Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->login_shelf_view()
      ->SetShowGuestButtonInOobe(show);
}

void LoginScreenController::SetShowParentAccessButton(bool show) {
  Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->login_shelf_view()
      ->SetShowParentAccessButton(show);
}

void LoginScreenController::SetShowParentAccessDialog(bool show) {
  login_data_dispatcher_.SetShowParentAccessDialog(show);
}

void LoginScreenController::FocusLoginShelf(bool reverse) {
  Shelf* shelf = Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow());
  // Tell the focus direction to the status area or the shelf so they can focus
  // the correct child view.
  if (reverse || !ShelfWidget::IsUsingViewsShelf()) {
    if (!Shell::GetPrimaryRootWindowController()->IsSystemTrayVisible())
      return;
    shelf->GetStatusAreaWidget()
        ->status_area_widget_delegate()
        ->set_default_last_focusable_child(reverse);
    Shell::Get()->focus_cycler()->FocusWidget(shelf->GetStatusAreaWidget());
  } else {
    shelf->shelf_widget()->set_default_last_focusable_child(reverse);
    Shell::Get()->focus_cycler()->FocusWidget(shelf->shelf_widget());
  }
}

void LoginScreenController::SetKioskApps(
    const std::vector<KioskAppMenuEntry>& kiosk_apps,
    const base::RepeatingCallback<void(const KioskAppMenuEntry&)>& launch_app) {
  Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->login_shelf_view()
      ->SetKioskApps(kiosk_apps, launch_app);
}

void LoginScreenController::SetAddUserButtonEnabled(bool enable) {
  Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->login_shelf_view()
      ->SetAddUserButtonEnabled(enable);
}

void LoginScreenController::SetShutdownButtonEnabled(bool enable) {
  Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())
      ->shelf_widget()
      ->login_shelf_view()
      ->SetShutdownButtonEnabled(enable);
}

void LoginScreenController::ShowResetScreen() {
  login_screen_client_->ShowResetScreen();
}

void LoginScreenController::ShowAccountAccessHelpApp() {
  login_screen_client_->ShowAccountAccessHelpApp();
}

void LoginScreenController::FocusOobeDialog() {
  if (!login_screen_client_)
    return;
  login_screen_client_->FocusOobeDialog();
}

void LoginScreenController::NotifyUserActivity() {
  if (!login_screen_client_)
    return;
  login_screen_client_->OnUserActivity();
}

void LoginScreenController::OnAuthenticateComplete(
    OnAuthenticateCallback callback,
    bool success) {
  authentication_stage_ = AuthenticationStage::kUserCallback;
  std::move(callback).Run(base::make_optional<bool>(success));
  authentication_stage_ = AuthenticationStage::kIdle;
}

void LoginScreenController::OnParentAccessValidationComplete(
    OnParentAccessValidation callback,
    bool success) {
  std::move(callback).Run(base::make_optional<bool>(success));
}

void LoginScreenController::OnShow() {
  SetSystemTrayVisibility(SystemTrayVisibility::kPrimary);
  if (authentication_stage_ != AuthenticationStage::kIdle) {
    AuthenticationStage authentication_stage = authentication_stage_;
    base::debug::Alias(&authentication_stage);
    LOG(FATAL) << "Unexpected authentication stage "
               << static_cast<int>(authentication_stage_);
  }
}

void LoginScreenController::OnFocusLeavingSystemTray(bool reverse) {
  if (!login_screen_client_)
    return;
  login_screen_client_->OnFocusLeavingSystemTray(reverse);
}

}  // namespace ash
