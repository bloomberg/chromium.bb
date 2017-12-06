// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/login_screen_controller.h"

#include "ash/login/lock_screen_apps_focus_observer.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/login/ui/login_data_dispatcher.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/login/auth/authpolicy_login_helper.h"
#include "chromeos/login/auth/user_context.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"

namespace ash {

namespace {

std::string CalculateHash(const std::string& password,
                          const std::string& salt,
                          chromeos::Key::KeyType key_type) {
  chromeos::Key key(password);
  key.Transform(key_type, salt);
  return key.GetSecret();
}

}  // namespace

LoginScreenController::LoginScreenController()
    : binding_(this), weak_factory_(this) {}

LoginScreenController::~LoginScreenController() = default;

// static
void LoginScreenController::RegisterProfilePrefs(PrefRegistrySimple* registry,
                                                 bool for_test) {
  if (for_test) {
    // There is no remote pref service, so pretend that ash owns the pref.
    registry->RegisterStringPref(prefs::kQuickUnlockPinSalt, "");
    return;
  }

  // Pref is owned by chrome and flagged as PUBLIC.
  registry->RegisterForeignPref(prefs::kQuickUnlockPinSalt);
}

void LoginScreenController::BindRequest(mojom::LoginScreenRequest request) {
  binding_.Bind(std::move(request));
}

void LoginScreenController::SetClient(mojom::LoginScreenClientPtr client) {
  login_screen_client_ = std::move(client);
}

void LoginScreenController::ShowLockScreen(ShowLockScreenCallback on_shown) {
  ash::LockScreen::Show(ash::LockScreen::ScreenType::kLock);
  std::move(on_shown).Run(true);
}

void LoginScreenController::ShowLoginScreen(ShowLoginScreenCallback on_shown) {
  // Login screen can only be used during login.
  if (Shell::Get()->session_controller()->GetSessionState() !=
      session_manager::SessionState::LOGIN_PRIMARY) {
    std::move(on_shown).Run(false);
    return;
  }

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

void LoginScreenController::ShowUserPodCustomIcon(
    const AccountId& account_id,
    mojom::EasyUnlockIconOptionsPtr icon) {
  DataDispatcher()->ShowEasyUnlockIcon(account_id, icon);
}

void LoginScreenController::HideUserPodCustomIcon(const AccountId& account_id) {
  auto icon_options = mojom::EasyUnlockIconOptions::New();
  icon_options->icon = mojom::EasyUnlockIconId::NONE;
  DataDispatcher()->ShowEasyUnlockIcon(account_id, icon_options);
}

void LoginScreenController::SetAuthType(
    const AccountId& account_id,
    proximity_auth::mojom::AuthType auth_type,
    const base::string16& initial_value) {
  if (auth_type == proximity_auth::mojom::AuthType::USER_CLICK) {
    DataDispatcher()->SetClickToUnlockEnabledForUser(account_id,
                                                     true /*enabled*/);
  } else {
    NOTIMPLEMENTED();
  }
}

void LoginScreenController::LoadUsers(
    std::vector<mojom::LoginUserInfoPtr> users,
    bool show_guest) {
  DCHECK(DataDispatcher());

  DataDispatcher()->NotifyUsers(users);
}

void LoginScreenController::SetPinEnabledForUser(const AccountId& account_id,
                                                 bool is_enabled) {
  // Chrome will update pin pod state every time user tries to authenticate.
  // LockScreen is destroyed in the case of authentication success.
  if (DataDispatcher())
    DataDispatcher()->SetPinEnabledForUser(account_id, is_enabled);
}

void LoginScreenController::SetDevChannelInfo(
    const std::string& os_version_label_text,
    const std::string& enterprise_info_text,
    const std::string& bluetooth_name) {
  if (DataDispatcher()) {
    DataDispatcher()->SetDevChannelInfo(os_version_label_text,
                                        enterprise_info_text, bluetooth_name);
  }
}

void LoginScreenController::AuthenticateUser(const AccountId& account_id,
                                             const std::string& password,
                                             bool authenticated_by_pin,
                                             OnAuthenticateCallback callback) {
  // Ignore concurrent auth attempts. This can happen if the user quickly enters
  // two separate passwords and hits enter.
  if (!login_screen_client_ || is_authenticating_) {
    LOG_IF(ERROR, is_authenticating_) << "Ignoring concurrent auth attempt";
    std::move(callback).Run(base::nullopt);
    return;
  }
  is_authenticating_ = true;

  // If auth is disabled by the debug overlay bypass the mojo call entirely, as
  // it will dismiss the lock screen if the password is correct.
  switch (force_fail_auth_for_debug_overlay_) {
    case ForceFailAuth::kOff:
      break;
    case ForceFailAuth::kImmediate:
      OnAuthenticateComplete(std::move(callback), false /*success*/);
      return;
    case ForceFailAuth::kDelayed:
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&LoginScreenController::OnAuthenticateComplete,
                         weak_factory_.GetWeakPtr(), base::Passed(&callback),
                         false),
          base::TimeDelta::FromSeconds(1));
      return;
  }

  // |DoAuthenticateUser| requires the system salt, so we fetch it first, and
  // then run |DoAuthenticateUser| as a continuation.
  auto do_authenticate = base::BindOnce(
      &LoginScreenController::DoAuthenticateUser, weak_factory_.GetWeakPtr(),
      account_id, password, authenticated_by_pin, std::move(callback));
  chromeos::SystemSaltGetter::Get()->GetSystemSalt(base::BindRepeating(
      &LoginScreenController::OnGetSystemSalt, weak_factory_.GetWeakPtr(),
      base::Passed(&do_authenticate)));
}

void LoginScreenController::HandleFocusLeavingLockScreenApps(bool reverse) {
  for (auto& observer : lock_screen_apps_focus_observers_)
    observer.OnFocusLeavingLockScreenApps(reverse);
}

void LoginScreenController::AttemptUnlock(const AccountId& account_id) {
  if (!login_screen_client_)
    return;
  login_screen_client_->AttemptUnlock(account_id);

  Shell::Get()->metrics()->login_metrics_recorder()->SetAuthMethod(
      LoginMetricsRecorder::AuthMethod::kSmartlock);
}

void LoginScreenController::HardlockPod(const AccountId& account_id) {
  if (!login_screen_client_)
    return;
  login_screen_client_->HardlockPod(account_id);
}

void LoginScreenController::RecordClickOnLockIcon(const AccountId& account_id) {
  if (!login_screen_client_)
    return;
  login_screen_client_->RecordClickOnLockIcon(account_id);
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

void LoginScreenController::AddLockScreenAppsFocusObserver(
    LockScreenAppsFocusObserver* observer) {
  lock_screen_apps_focus_observers_.AddObserver(observer);
}

void LoginScreenController::RemoveLockScreenAppsFocusObserver(
    LockScreenAppsFocusObserver* observer) {
  lock_screen_apps_focus_observers_.RemoveObserver(observer);
}

void LoginScreenController::FlushForTesting() {
  login_screen_client_.FlushForTesting();
}

void LoginScreenController::DoAuthenticateUser(const AccountId& account_id,
                                               const std::string& password,
                                               bool authenticated_by_pin,
                                               OnAuthenticateCallback callback,
                                               const std::string& system_salt) {
  int dummy_value;
  bool is_pin =
      authenticated_by_pin && base::StringToInt(password, &dummy_value);
  std::string hashed_password = CalculateHash(
      password, system_salt, chromeos::Key::KEY_TYPE_SALTED_SHA256_TOP_HALF);

  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (is_pin && prefs) {
    hashed_password =
        CalculateHash(password, prefs->GetString(prefs::kQuickUnlockPinSalt),
                      chromeos::Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234);
  }

  if (account_id.GetAccountType() == AccountType::ACTIVE_DIRECTORY && !is_pin) {
    // Try to get kerberos TGT while we have user's password typed on the lock
    // screen. Using invalid/bad password is fine. Failure to get TGT here is OK
    // - that could mean e.g. Active Directory server is not reachable.
    // AuthPolicyCredentialsManager regularly checks TGT status inside the user
    // session.
    chromeos::AuthPolicyLoginHelper::TryAuthenticateUser(
        account_id.GetUserEmail(), account_id.GetObjGuid(), password);
  }

  Shell::Get()->metrics()->login_metrics_recorder()->SetAuthMethod(
      is_pin ? LoginMetricsRecorder::AuthMethod::kPin
             : LoginMetricsRecorder::AuthMethod::kPassword);
  login_screen_client_->AuthenticateUser(
      account_id, hashed_password, is_pin,
      base::BindOnce(&LoginScreenController::OnAuthenticateComplete,
                     weak_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void LoginScreenController::OnAuthenticateComplete(
    OnAuthenticateCallback callback,
    bool success) {
  is_authenticating_ = false;
  std::move(callback).Run(success);
}

void LoginScreenController::OnGetSystemSalt(PendingDoAuthenticateUser then,
                                            const std::string& system_salt) {
  std::move(then).Run(system_salt);
}

LoginDataDispatcher* LoginScreenController::DataDispatcher() const {
  if (!ash::LockScreen::IsShown())
    return nullptr;
  return ash::LockScreen::Get()->data_dispatcher();
}

}  // namespace ash
