// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login_screen_client.h"

#include <utility>

#include "ash/public/interfaces/constants.mojom.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/reauth_stats.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client.h"
#include "components/user_manager/remove_user_delegate.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace {
LoginScreenClient* g_login_screen_client_instance = nullptr;
}  // namespace

LoginScreenClient::Delegate::Delegate() = default;
LoginScreenClient::Delegate::~Delegate() = default;

LoginScreenClient::LoginScreenClient() : binding_(this) {
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &login_screen_);
  // Register this object as the client interface implementation.
  ash::mojom::LoginScreenClientPtr client;
  binding_.Bind(mojo::MakeRequest(&client));
  login_screen_->SetClient(std::move(client));

  DCHECK(!g_login_screen_client_instance);
  g_login_screen_client_instance = this;
}

LoginScreenClient::~LoginScreenClient() {
  DCHECK_EQ(this, g_login_screen_client_instance);
  g_login_screen_client_instance = nullptr;
}

// static
bool LoginScreenClient::HasInstance() {
  return !!g_login_screen_client_instance;
}

// static
LoginScreenClient* LoginScreenClient::Get() {
  DCHECK(g_login_screen_client_instance);
  return g_login_screen_client_instance;
}

void LoginScreenClient::AuthenticateUser(
    const AccountId& account_id,
    const std::string& hashed_password,
    const password_manager::SyncPasswordData& sync_password_data,
    bool authenticated_by_pin,
    AuthenticateUserCallback callback) {
  if (delegate_) {
    delegate_->HandleAuthenticateUser(account_id, hashed_password,
                                      sync_password_data, authenticated_by_pin,
                                      std::move(callback));
  } else {
    LOG(ERROR) << "Returning failed authentication attempt; no delegate";
    std::move(callback).Run(false);
  }
}

void LoginScreenClient::ShowLockScreen(
    ash::mojom::LoginScreen::ShowLockScreenCallback on_shown) {
  login_screen_->ShowLockScreen(std::move(on_shown));
}

void LoginScreenClient::ShowLoginScreen(
    ash::mojom::LoginScreen::ShowLoginScreenCallback on_shown) {
  login_screen_->ShowLoginScreen(std::move(on_shown));
}

void LoginScreenClient::AttemptUnlock(const AccountId& account_id) {
  if (delegate_)
    delegate_->HandleAttemptUnlock(account_id);
}

void LoginScreenClient::HardlockPod(const AccountId& account_id) {
  if (delegate_)
    delegate_->HandleHardlockPod(account_id);
}

void LoginScreenClient::RecordClickOnLockIcon(const AccountId& account_id) {
  if (delegate_)
    delegate_->HandleRecordClickOnLockIcon(account_id);
}

void LoginScreenClient::OnFocusPod(const AccountId& account_id) {
  if (delegate_)
    delegate_->HandleOnFocusPod(account_id);
}

void LoginScreenClient::OnNoPodFocused() {
  if (delegate_)
    delegate_->HandleOnNoPodFocused();
}

void LoginScreenClient::FocusLockScreenApps(bool reverse) {
  // If delegate is not set, or it fails to handle focus request, call
  // |HandleFocusLeavingLockScreenApps| so the lock screen mojo service can
  // give focus to the next window in the tab order.
  if (!delegate_ || !delegate_->HandleFocusLockScreenApps(reverse))
    HandleFocusLeavingLockScreenApps(reverse);
}

void LoginScreenClient::ShowGaiaSignin() {
  if (chromeos::LoginDisplayHost::default_host()) {
    chromeos::LoginDisplayHost::default_host()->UpdateGaiaDialogVisibility(
        true /*visible*/);
  }
}

void LoginScreenClient::OnRemoveUserWarningShown() {
  ProfileMetrics::LogProfileDeleteUser(
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER_SHOW_WARNING);
}

void LoginScreenClient::RemoveUser(const AccountId& account_id) {
  ProfileMetrics::LogProfileDeleteUser(
      ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
  user_manager::UserManager::Get()->RemoveUser(account_id,
                                               nullptr /*delegate*/);
}

void LoginScreenClient::LoadWallpaper(const AccountId& account_id) {
  WallpaperControllerClient::Get()->ShowUserWallpaper(account_id);
}

void LoginScreenClient::SignOutUser() {
  chromeos::ScreenLocker::default_screen_locker()->Signout();
}

void LoginScreenClient::CancelAddUser() {
  chromeos::UserAddingScreen::Get()->Cancel();
}

void LoginScreenClient::LoginAsGuest() {
  if (delegate_)
    delegate_->HandleLoginAsGuest();
}

void LoginScreenClient::OnMaxIncorrectPasswordAttempted(
    const AccountId& account_id) {
  RecordReauthReason(account_id,
                     chromeos::ReauthReason::INCORRECT_PASSWORD_ENTERED);
}

void LoginScreenClient::ShowErrorMessage(int32_t login_attempts,
                                         const std::string& error_text,
                                         const std::string& help_link_text,
                                         int32_t help_topic_id) {
  login_screen_->ShowErrorMessage(login_attempts, error_text, help_link_text,
                                  help_topic_id);
}

void LoginScreenClient::ClearErrors() {
  login_screen_->ClearErrors();
}

void LoginScreenClient::ShowUserPodCustomIcon(
    const AccountId& account_id,
    ash::mojom::EasyUnlockIconOptionsPtr icon) {
  login_screen_->ShowUserPodCustomIcon(account_id, std::move(icon));
}

void LoginScreenClient::HideUserPodCustomIcon(const AccountId& account_id) {
  login_screen_->HideUserPodCustomIcon(account_id);
}

void LoginScreenClient::SetAuthType(const AccountId& account_id,
                                    proximity_auth::mojom::AuthType auth_type,
                                    const base::string16& initial_value) {
  login_screen_->SetAuthType(account_id, auth_type, initial_value);
}

void LoginScreenClient::LoadUsers(
    std::vector<ash::mojom::LoginUserInfoPtr> users_list,
    bool show_guest) {
  login_screen_->LoadUsers(std::move(users_list), show_guest);
}

void LoginScreenClient::SetPinEnabledForUser(const AccountId& account_id,
                                             bool is_enabled) {
  login_screen_->SetPinEnabledForUser(account_id, is_enabled);
}

void LoginScreenClient::HandleFocusLeavingLockScreenApps(bool reverse) {
  login_screen_->HandleFocusLeavingLockScreenApps(reverse);
}

void LoginScreenClient::SetDevChannelInfo(
    const std::string& os_version_label_text,
    const std::string& enterprise_info_text,
    const std::string& bluetooth_name) {
  login_screen_->SetDevChannelInfo(os_version_label_text, enterprise_info_text,
                                   bluetooth_name);
}

void LoginScreenClient::IsReadyForPassword(
    ash::mojom::LoginScreen::IsReadyForPasswordCallback callback) {
  login_screen_->IsReadyForPassword(std::move(callback));
}

void LoginScreenClient::SetPublicSessionDisplayName(
    const AccountId& account_id,
    const std::string& display_name) {
  login_screen_->SetPublicSessionDisplayName(account_id, display_name);
}

void LoginScreenClient::SetPublicSessionLocales(
    const AccountId& account_id,
    std::unique_ptr<base::ListValue> locales,
    const std::string& default_locale,
    bool show_advanced_view) {
  login_screen_->SetPublicSessionLocales(account_id, std::move(locales),
                                         default_locale, show_advanced_view);
}

void LoginScreenClient::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}
