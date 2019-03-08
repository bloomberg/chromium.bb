// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/test_login_screen.h"

#include <utility>

#include "ash/public/interfaces/constants.mojom.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_filter.h"

TestLoginScreen::TestLoginScreen() {
  CHECK(content::ServiceManagerConnection::GetForProcess())
      << "ServiceManager is uninitialized. Did you forget to create a "
         "content::TestServiceManagerContext?";
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->OverrideBinderForTesting(
          service_manager::ServiceFilter::ByName(ash::mojom::kServiceName),
          ash::mojom::LoginScreen::Name_,
          base::BindRepeating(&TestLoginScreen::Bind, base::Unretained(this)));
}

TestLoginScreen::~TestLoginScreen() {
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->ClearBinderOverrideForTesting(
          service_manager::ServiceFilter::ByName(ash::mojom::kServiceName),
          ash::mojom::LoginScreen::Name_);
}

void TestLoginScreen::SetClient(ash::mojom::LoginScreenClientPtr client) {}

void TestLoginScreen::ShowLockScreen(ShowLockScreenCallback callback) {
  std::move(callback).Run(true);
}

void TestLoginScreen::ShowLoginScreen(ShowLoginScreenCallback callback) {
  std::move(callback).Run(true);
}

void TestLoginScreen::ShowErrorMessage(int32_t login_attempts,
                                       const std::string& error_text,
                                       const std::string& help_link_text,
                                       int32_t help_topic_id) {}

void TestLoginScreen::ShowWarningBanner(const base::string16& message) {}

void TestLoginScreen::HideWarningBanner() {}

void TestLoginScreen::ClearErrors() {}

void TestLoginScreen::ShowUserPodCustomIcon(
    const AccountId& account_id,
    ::ash::mojom::EasyUnlockIconOptionsPtr icon) {}

void TestLoginScreen::HideUserPodCustomIcon(const AccountId& account_id) {}

void TestLoginScreen::SetAuthType(const AccountId& account_id,
                                  ::proximity_auth::mojom::AuthType auth_type,
                                  const base::string16& initial_value) {}

void TestLoginScreen::SetUserList(
    std::vector<::ash::mojom::LoginUserInfoPtr> users) {}

void TestLoginScreen::SetPinEnabledForUser(const AccountId& account_id,
                                           bool is_enabled) {}

void TestLoginScreen::SetFingerprintState(
    const AccountId& account_id,
    ::ash::mojom::FingerprintState state) {}

void TestLoginScreen::NotifyFingerprintAuthResult(const AccountId& account_id,
                                                  bool successful) {}

void TestLoginScreen::SetAvatarForUser(const AccountId& account_id,
                                       ::ash::mojom::UserAvatarPtr avatar) {}

void TestLoginScreen::SetAuthEnabledForUser(
    const AccountId& account_id,
    bool is_enabled,
    base::Optional<base::Time> auth_reenabled_time) {}

void TestLoginScreen::HandleFocusLeavingLockScreenApps(bool reverse) {}

void TestLoginScreen::SetSystemInfo(bool show_if_hidden,
                                    const std::string& os_version_label_text,
                                    const std::string& enterprise_info_text,
                                    const std::string& bluetooth_name) {}

void TestLoginScreen::IsReadyForPassword(IsReadyForPasswordCallback callback) {
  std::move(callback).Run(true);
}

void TestLoginScreen::SetPublicSessionDisplayName(
    const AccountId& account_id,
    const std::string& display_name) {}

void TestLoginScreen::SetPublicSessionLocales(
    const AccountId& account_id,
    std::vector<::ash::mojom::LocaleItemPtr> locales,
    const std::string& default_locale,
    bool show_advanced_view) {}

void TestLoginScreen::SetPublicSessionKeyboardLayouts(
    const AccountId& account_id,
    const std::string& locale,
    std::vector<::ash::mojom::InputMethodItemPtr> keyboard_layouts) {}

void TestLoginScreen::SetPublicSessionShowFullManagementDisclosure(
    bool show_full_management_disclosure) {}

void TestLoginScreen::SetKioskApps(
    std::vector<::ash::mojom::KioskAppInfoPtr> kiosk_apps,
    SetKioskAppsCallback callback) {}

void TestLoginScreen::ShowKioskAppError(const std::string& message) {}

void TestLoginScreen::NotifyOobeDialogState(ash::mojom::OobeDialogState state) {
}

void TestLoginScreen::SetAddUserButtonEnabled(bool enable) {}

void TestLoginScreen::SetShutdownButtonEnabled(bool enable) {}

void TestLoginScreen::SetAllowLoginAsGuest(bool allow_guest) {}

void TestLoginScreen::SetShowGuestButtonInOobe(bool show) {}

void TestLoginScreen::SetShowParentAccessButton(bool show) {}

void TestLoginScreen::SetShowParentAccessDialog(bool show) {}

void TestLoginScreen::FocusLoginShelf(bool reverse) {}

void TestLoginScreen::Bind(mojo::ScopedMessagePipeHandle handle) {
  binding_.Bind(ash::mojom::LoginScreenRequest(std::move(handle)));
}
