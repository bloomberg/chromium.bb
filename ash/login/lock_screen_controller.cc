// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/lock_screen_controller.h"

#include "ash/login/ui/lock_screen.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/login/auth/user_context.h"

namespace ash {

LockScreenController::LockScreenController() = default;

LockScreenController::~LockScreenController() = default;

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
  NOTIMPLEMENTED();
}

void LockScreenController::SetPinEnabledForUser(const AccountId& account_id,
                                                bool is_enabled) {
  ash::LockScreen::Get()->SetPinEnabledForUser(account_id, is_enabled);
}

void LockScreenController::AuthenticateUser(
    const AccountId& account_id,
    const std::string& password,
    bool authenticated_by_pin,
    mojom::LockScreenClient::AuthenticateUserCallback callback) {
  if (!lock_screen_client_)
    return;

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

void LockScreenController::OnMaxIncorrectPasswordAttempted(
    const AccountId& account_id) {
  if (!lock_screen_client_)
    return;
  lock_screen_client_->OnMaxIncorrectPasswordAttempted(account_id);
}

void LockScreenController::DoAuthenticateUser(
    const AccountId& account_id,
    const std::string& password,
    bool authenticated_by_pin,
    mojom::LockScreenClient::AuthenticateUserCallback callback,
    const std::string& system_salt) {
  // Hash password before sending through mojo.
  // TODO(xiaoyinh): Pin is hashed differently by using a different salt and
  // a different hash algorithm. Update this part in PinStorage.
  chromeos::Key key(password);
  key.Transform(chromeos::Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, system_salt);
  lock_screen_client_->AuthenticateUser(
      account_id, key.GetSecret(), authenticated_by_pin, std::move(callback));
}

void LockScreenController::OnGetSystemSalt(const std::string& system_salt) {
  std::move(pending_user_auth_).Run(system_salt);
}

}  // namespace ash
