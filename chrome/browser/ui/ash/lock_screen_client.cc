// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/lock_screen_client.h"

#include "ash/public/interfaces/constants.mojom.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {
LockScreenClient* g_instance = nullptr;
}  // namespace

LockScreenClient::Delegate::Delegate() = default;
LockScreenClient::Delegate::~Delegate() = default;

LockScreenClient::LockScreenClient() : binding_(this) {
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &lock_screen_);
  // Register this object as the client interface implementation.
  ash::mojom::LockScreenClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  lock_screen_->SetClient(std::move(client));

  DCHECK(!g_instance);
  g_instance = this;
}

LockScreenClient::~LockScreenClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
LockScreenClient* LockScreenClient::Get() {
  return g_instance;
}

void LockScreenClient::AuthenticateUser(const AccountId& account_id,
                                        const std::string& hashed_password,
                                        bool authenticated_by_pin,
                                        AuthenticateUserCallback callback) {
  // TODO(xiaoyinh): Complete the implementation below.
  // It should be similar as SigninScreenHandler::HandleAuthenticateUser.
  chromeos::UserContext user_context(account_id);
  chromeos::Key key(chromeos::Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                    std::string(), hashed_password);
  user_context.SetKey(key);
  user_context.SetIsUsingPin(authenticated_by_pin);
  chromeos::ScreenLocker::default_screen_locker()->Authenticate(
      user_context, std::move(callback));
}

void LockScreenClient::ShowLockScreen(
    ash::mojom::LockScreen::ShowLockScreenCallback on_shown) {
  lock_screen_->ShowLockScreen(std::move(on_shown));
}

void LockScreenClient::AttemptUnlock(const AccountId& account_id) {
  if (!delegate_)
    return;
  delegate_->HandleAttemptUnlock(account_id);
}

void LockScreenClient::HardlockPod(const AccountId& account_id) {
  if (!delegate_)
    return;
  delegate_->HandleHardlockPod(account_id);
}

void LockScreenClient::RecordClickOnLockIcon(const AccountId& account_id) {
  if (!delegate_)
    return;
  delegate_->HandleRecordClickOnLockIcon(account_id);
}

void LockScreenClient::ShowErrorMessage(int32_t login_attempts,
                                        const std::string& error_text,
                                        const std::string& help_link_text,
                                        int32_t help_topic_id) {
  lock_screen_->ShowErrorMessage(login_attempts, error_text, help_link_text,
                                 help_topic_id);
}

void LockScreenClient::ClearErrors() {
  lock_screen_->ClearErrors();
}

void LockScreenClient::ShowUserPodCustomIcon(
    const AccountId& account_id,
    ash::mojom::UserPodCustomIconOptionsPtr icon) {
  lock_screen_->ShowUserPodCustomIcon(account_id, std::move(icon));
}

void LockScreenClient::HideUserPodCustomIcon(const AccountId& account_id) {
  lock_screen_->HideUserPodCustomIcon(account_id);
}

void LockScreenClient::SetAuthType(const AccountId& account_id,
                                   ash::mojom::AuthType auth_type,
                                   const base::string16& initial_value) {
  lock_screen_->SetAuthType(account_id, auth_type, initial_value);
}

void LockScreenClient::LoadUsers(std::unique_ptr<base::ListValue> users_list,
                                 bool show_guest) {
  lock_screen_->LoadUsers(std::move(users_list), show_guest);
}

void LockScreenClient::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}
