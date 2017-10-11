// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/lock_screen_controller.h"

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

LockScreenController::LockScreenController() = default;
LockScreenController::~LockScreenController() = default;

// static
void LockScreenController::RegisterProfilePrefs(PrefRegistrySimple* registry,
                                                bool for_test) {
  if (for_test) {
    // There is no remote pref service, so pretend that ash owns the pref.
    registry->RegisterStringPref(prefs::kQuickUnlockPinSalt, "");
    return;
  }

  // Pref is owned by chrome and flagged as PUBLIC.
  registry->RegisterForeignPref(prefs::kQuickUnlockPinSalt);
}

void LockScreenController::BindRequest(mojom::LockScreenRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void LockScreenController::SetClient(mojom::LockScreenClientPtr client) {
  lock_screen_client_ = std::move(client);
}

void LockScreenController::ShowLockScreen(ShowLockScreenCallback on_shown) {
  ash::LockScreen::Show();
  std::move(on_shown).Run(true);
}

void LockScreenController::ShowErrorMessage(int32_t login_attempts,
                                            const std::string& error_text,
                                            const std::string& help_link_text,
                                            int32_t help_topic_id) {
  NOTIMPLEMENTED();
}

void LockScreenController::ClearErrors() {
  NOTIMPLEMENTED();
}

void LockScreenController::ShowUserPodCustomIcon(
    const AccountId& account_id,
    mojom::UserPodCustomIconOptionsPtr icon) {
  NOTIMPLEMENTED();
}

void LockScreenController::HideUserPodCustomIcon(const AccountId& account_id) {
  NOTIMPLEMENTED();
}

void LockScreenController::SetAuthType(
    const AccountId& account_id,
    proximity_auth::mojom::AuthType auth_type,
    const base::string16& initial_value) {
  NOTIMPLEMENTED();
}

void LockScreenController::LoadUsers(std::vector<mojom::LoginUserInfoPtr> users,
                                     bool show_guest) {
  DCHECK(DataDispatcher());

  DataDispatcher()->NotifyUsers(users);
}

void LockScreenController::SetPinEnabledForUser(const AccountId& account_id,
                                                bool is_enabled) {
  // Chrome will update pin pod state every time user tries to authenticate.
  // LockScreen is destroyed in the case of authentication success.
  if (DataDispatcher())
    DataDispatcher()->SetPinEnabledForUser(account_id, is_enabled);
}

void LockScreenController::AuthenticateUser(
    const AccountId& account_id,
    const std::string& password,
    bool authenticated_by_pin,
    mojom::LockScreenClient::AuthenticateUserCallback callback) {
  if (!lock_screen_client_)
    return;

  // If auth is disabled by the debug overlay bypass the mojo call entirely, as
  // it will dismiss the lock screen if the password is correct.
  if (force_fail_auth_for_debug_overlay_) {
    std::move(callback).Run(false);
    return;
  }

  // We cannot execute auth requests directly via GetSystemSalt because it
  // expects a base::Callback instance, but |callback| is a base::OnceCallback.
  // Instead, we store |callback| on this object and invoke it locally once we
  // have the system salt.
  DCHECK(!pending_user_auth_) << "More than one concurrent auth attempt";
  pending_user_auth_ = base::BindOnce(
      &LockScreenController::DoAuthenticateUser, base::Unretained(this),
      account_id, password, authenticated_by_pin, std::move(callback));
  chromeos::SystemSaltGetter::Get()->GetSystemSalt(base::Bind(
      &LockScreenController::OnGetSystemSalt, base::Unretained(this)));
}

void LockScreenController::HandleFocusLeavingLockScreenApps(bool reverse) {
  for (auto& observer : lock_screen_apps_focus_observers_)
    observer.OnFocusLeavingLockScreenApps(reverse);
}

void LockScreenController::AttemptUnlock(const AccountId& account_id) {
  if (!lock_screen_client_)
    return;
  lock_screen_client_->AttemptUnlock(account_id);
}

void LockScreenController::HardlockPod(const AccountId& account_id) {
  if (!lock_screen_client_)
    return;
  lock_screen_client_->HardlockPod(account_id);
}

void LockScreenController::RecordClickOnLockIcon(const AccountId& account_id) {
  if (!lock_screen_client_)
    return;
  lock_screen_client_->RecordClickOnLockIcon(account_id);
}

void LockScreenController::OnFocusPod(const AccountId& account_id) {
  if (!lock_screen_client_)
    return;
  lock_screen_client_->OnFocusPod(account_id);
}

void LockScreenController::OnNoPodFocused() {
  if (!lock_screen_client_)
    return;
  lock_screen_client_->OnNoPodFocused();
}

void LockScreenController::LoadWallpaper(const AccountId& account_id) {
  if (!lock_screen_client_)
    return;
  lock_screen_client_->LoadWallpaper(account_id);
}

void LockScreenController::SignOutUser() {
  if (!lock_screen_client_)
    return;
  lock_screen_client_->SignOutUser();
}

void LockScreenController::CancelAddUser() {
  if (!lock_screen_client_)
    return;
  lock_screen_client_->CancelAddUser();
}

void LockScreenController::OnMaxIncorrectPasswordAttempted(
    const AccountId& account_id) {
  if (!lock_screen_client_)
    return;
  lock_screen_client_->OnMaxIncorrectPasswordAttempted(account_id);
}

void LockScreenController::FocusLockScreenApps(bool reverse) {
  if (!lock_screen_client_)
    return;
  lock_screen_client_->FocusLockScreenApps(reverse);
}

void LockScreenController::AddLockScreenAppsFocusObserver(
    LockScreenAppsFocusObserver* observer) {
  lock_screen_apps_focus_observers_.AddObserver(observer);
}

void LockScreenController::RemoveLockScreenAppsFocusObserver(
    LockScreenAppsFocusObserver* observer) {
  lock_screen_apps_focus_observers_.RemoveObserver(observer);
}

void LockScreenController::FlushForTesting() {
  lock_screen_client_.FlushForTesting();
}

void LockScreenController::DoAuthenticateUser(
    const AccountId& account_id,
    const std::string& password,
    bool authenticated_by_pin,
    mojom::LockScreenClient::AuthenticateUserCallback callback,
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

  lock_screen_client_->AuthenticateUser(account_id, hashed_password, is_pin,
                                        std::move(callback));
}

void LockScreenController::OnGetSystemSalt(const std::string& system_salt) {
  std::move(pending_user_auth_).Run(system_salt);
}

LoginDataDispatcher* LockScreenController::DataDispatcher() const {
  if (!ash::LockScreen::IsShown())
    return nullptr;
  return ash::LockScreen::Get()->data_dispatcher();
}

}  // namespace ash
