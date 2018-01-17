// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LOGIN_SCREEN_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_LOGIN_SCREEN_CLIENT_H_

#include "ash/public/interfaces/login_screen.mojom.h"
#include "base/macros.h"
#include "components/password_manager/public/interfaces/sync_password_data.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/base/ime/chromeos/input_method_manager.h"

using AuthenticateUserCallback =
    ash::mojom::LoginScreenClient::AuthenticateUserCallback;

// Handles method calls sent from ash to chrome. Also sends messages from chrome
// to ash.
class LoginScreenClient : public ash::mojom::LoginScreenClient {
 public:
  LoginScreenClient();
  ~LoginScreenClient() override;

  // Handles method calls coming from ash into chrome.
  class Delegate {
   public:
    Delegate();
    virtual ~Delegate();
    virtual void HandleAuthenticateUser(
        const AccountId& account_id,
        const std::string& hashed_password,
        const password_manager::SyncPasswordData& sync_password_data,
        bool authenticated_by_pin,
        AuthenticateUserCallback callback) = 0;
    virtual void HandleAttemptUnlock(const AccountId& account_id) = 0;
    virtual void HandleHardlockPod(const AccountId& account_id) = 0;
    virtual void HandleRecordClickOnLockIcon(const AccountId& account_id) = 0;
    virtual void HandleOnFocusPod(const AccountId& account_id) = 0;
    virtual void HandleOnNoPodFocused() = 0;
    // Handles request to focus a lock screen app window. Returns whether the
    // focus has been handed over to a lock screen app. For example, this might
    // fail if a hander for lock screen apps focus has not been set.
    virtual bool HandleFocusLockScreenApps(bool reverse) = 0;
    virtual void HandleLoginAsGuest() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  static bool HasInstance();
  static LoginScreenClient* Get();

  // ash::mojom::LoginScreenClient:
  void AuthenticateUser(
      const AccountId& account_id,
      const std::string& hashed_password,
      const password_manager::SyncPasswordData& sync_password_data,
      bool authenticated_by_pin,
      AuthenticateUserCallback callback) override;
  void AttemptUnlock(const AccountId& account_id) override;
  void HardlockPod(const AccountId& account_id) override;
  void RecordClickOnLockIcon(const AccountId& account_id) override;
  void OnFocusPod(const AccountId& account_id) override;
  void OnNoPodFocused() override;
  void LoadWallpaper(const AccountId& account_id) override;
  void SignOutUser() override;
  void CancelAddUser() override;
  void LoginAsGuest() override;
  void OnMaxIncorrectPasswordAttempted(const AccountId& account_id) override;
  void FocusLockScreenApps(bool reverse) override;

  // Wrappers around the mojom::LockScreen interface.
  void ShowLockScreen(ash::mojom::LoginScreen::ShowLockScreenCallback on_shown);
  void ShowLoginScreen(
      ash::mojom::LoginScreen::ShowLoginScreenCallback on_shown);
  void ShowErrorMessage(int32_t login_attempts,
                        const std::string& error_text,
                        const std::string& help_link_text,
                        int32_t help_topic_id);
  void ClearErrors();
  void ShowUserPodCustomIcon(const AccountId& account_id,
                             ash::mojom::EasyUnlockIconOptionsPtr icon);
  void HideUserPodCustomIcon(const AccountId& account_id);
  void SetAuthType(const AccountId& account_id,
                   proximity_auth::mojom::AuthType auth_type,
                   const base::string16& initial_value);
  void LoadUsers(std::vector<ash::mojom::LoginUserInfoPtr> users_list,
                 bool show_guest);
  void SetPinEnabledForUser(const AccountId& account_id, bool is_enabled);
  void HandleFocusLeavingLockScreenApps(bool reverse);
  void SetDevChannelInfo(const std::string& os_version_label_text,
                         const std::string& enterprise_info_text,
                         const std::string& bluetooth_name);
  void IsReadyForPassword(
      ash::mojom::LoginScreen::IsReadyForPasswordCallback callback);

  void SetDelegate(Delegate* delegate);

 private:
  // Lock screen mojo service in ash.
  ash::mojom::LoginScreenPtr login_screen_;

  // Binds this object to the client interface.
  mojo::Binding<ash::mojom::LoginScreenClient> binding_;
  Delegate* delegate_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(LoginScreenClient);
};

#endif  // CHROME_BROWSER_UI_ASH_LOGIN_SCREEN_CLIENT_H_
