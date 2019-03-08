// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_TEST_LOGIN_SCREEN_H_
#define CHROME_BROWSER_UI_ASH_TEST_LOGIN_SCREEN_H_

#include <string>
#include <vector>

#include "ash/public/interfaces/login_screen.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"

// Test implementation of ash's mojo LoginScreen interface.
//
// Registers itself to ServiceManager on construction and deregisters
// on destruction.
//
// Note: A ServiceManagerConnection must be initialized before constructing this
// object. Consider using content::TestServiceManagerContext on your tests.
class TestLoginScreen : public ash::mojom::LoginScreen {
 public:
  TestLoginScreen();
  ~TestLoginScreen() override;

  // ash:mojom::LoginScreen:
  void SetClient(ash::mojom::LoginScreenClientPtr client) override;
  void ShowLockScreen(ShowLockScreenCallback callback) override;
  void ShowLoginScreen(ShowLoginScreenCallback callback) override;
  void ShowErrorMessage(int32_t login_attempts,
                        const std::string& error_text,
                        const std::string& help_link_text,
                        int32_t help_topic_id) override;
  void ShowWarningBanner(const base::string16& message) override;
  void HideWarningBanner() override;
  void ClearErrors() override;
  void ShowUserPodCustomIcon(
      const AccountId& account_id,
      ::ash::mojom::EasyUnlockIconOptionsPtr icon) override;
  void HideUserPodCustomIcon(const AccountId& account_id) override;
  void SetAuthType(const AccountId& account_id,
                   ::proximity_auth::mojom::AuthType auth_type,
                   const base::string16& initial_value) override;
  void SetUserList(std::vector<::ash::mojom::LoginUserInfoPtr> users) override;

  void SetPinEnabledForUser(const AccountId& account_id,
                            bool is_enabled) override;
  void SetFingerprintState(const AccountId& account_id,
                           ::ash::mojom::FingerprintState state) override;
  void NotifyFingerprintAuthResult(const AccountId& account_id,
                                   bool successful) override;
  void SetAvatarForUser(const AccountId& account_id,
                        ::ash::mojom::UserAvatarPtr avatar) override;
  void SetAuthEnabledForUser(
      const AccountId& account_id,
      bool is_enabled,
      base::Optional<base::Time> auth_reenabled_time) override;
  void HandleFocusLeavingLockScreenApps(bool reverse) override;
  void SetSystemInfo(bool show_if_hidden,
                     const std::string& os_version_label_text,
                     const std::string& enterprise_info_text,
                     const std::string& bluetooth_name) override;
  void IsReadyForPassword(IsReadyForPasswordCallback callback) override;
  void SetPublicSessionDisplayName(const AccountId& account_id,
                                   const std::string& display_name) override;
  void SetPublicSessionLocales(const AccountId& account_id,
                               std::vector<::ash::mojom::LocaleItemPtr> locales,
                               const std::string& default_locale,
                               bool show_advanced_view) override;
  void SetPublicSessionKeyboardLayouts(
      const AccountId& account_id,
      const std::string& locale,
      std::vector<::ash::mojom::InputMethodItemPtr> keyboard_layouts) override;
  void SetPublicSessionShowFullManagementDisclosure(
      bool show_full_management_disclosure) override;
  void SetKioskApps(std::vector<::ash::mojom::KioskAppInfoPtr> kiosk_apps,
                    SetKioskAppsCallback callback) override;
  void ShowKioskAppError(const std::string& message) override;
  void NotifyOobeDialogState(ash::mojom::OobeDialogState state) override;
  void SetAddUserButtonEnabled(bool enable) override;
  void SetShutdownButtonEnabled(bool enable) override;
  void SetAllowLoginAsGuest(bool allow_guest) override;
  void SetShowGuestButtonInOobe(bool show) override;
  void SetShowParentAccessButton(bool show) override;
  void SetShowParentAccessDialog(bool show) override;
  void FocusLoginShelf(bool reverse) override;

 private:
  void Bind(mojo::ScopedMessagePipeHandle handle);
  mojo::Binding<ash::mojom::LoginScreen> binding_{this};

  DISALLOW_COPY_AND_ASSIGN(TestLoginScreen);
};

#endif  // CHROME_BROWSER_UI_ASH_TEST_LOGIN_SCREEN_H_
