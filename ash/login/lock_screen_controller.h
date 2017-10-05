// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_LOCK_SCREEN_CONTROLLER_H_
#define ASH_LOGIN_LOCK_SCREEN_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/interfaces/lock_screen.mojom.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/binding_set.h"

class PrefRegistrySimple;

namespace ash {

class LockScreenAppsFocusObserver;
class LoginDataDispatcher;

// LockScreenController implements mojom::LockScreen and wraps the
// mojom::LockScreenClient interface. This lets a consumer of ash provide a
// LockScreenClient, which we will dispatch to if one has been provided to us.
// This could send requests to LockScreenClient and also handle requests from
// LockScreenClient through mojo.
class ASH_EXPORT LockScreenController : public mojom::LockScreen {
 public:
  using OnShownCallback = base::OnceCallback<void(bool did_show)>;

  LockScreenController();
  ~LockScreenController() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry, bool for_test);

  // Binds the mojom::LockScreen interface to this object.
  void BindRequest(mojom::LockScreenRequest request);

  // mojom::LockScreen:
  void SetClient(mojom::LockScreenClientPtr client) override;
  void ShowLockScreen(ShowLockScreenCallback callback) override;
  void ShowErrorMessage(int32_t login_attempts,
                        const std::string& error_text,
                        const std::string& help_link_text,
                        int32_t help_topic_id) override;
  void ClearErrors() override;
  void ShowUserPodCustomIcon(const AccountId& account_id,
                             mojom::UserPodCustomIconOptionsPtr icon) override;
  void HideUserPodCustomIcon(const AccountId& account_id) override;
  void SetAuthType(const AccountId& account_id,
                   proximity_auth::mojom::AuthType auth_type,
                   const base::string16& initial_value) override;
  void LoadUsers(std::vector<mojom::LoginUserInfoPtr> users,
                 bool show_guest) override;
  void SetPinEnabledForUser(const AccountId& account_id,
                            bool is_enabled) override;
  void HandleFocusLeavingLockScreenApps(bool reverse) override;

  // Wrappers around the mojom::LockScreenClient interface.
  // Hash the password and send AuthenticateUser request to LockScreenClient.
  // LockScreenClient(chrome) will do the authentication and request to show
  // error messages in the lock screen if auth fails, or request to clear
  // errors if auth succeeds.
  void AuthenticateUser(
      const AccountId& account_id,
      const std::string& password,
      bool authenticated_by_pin,
      mojom::LockScreenClient::AuthenticateUserCallback callback);
  void AttemptUnlock(const AccountId& account_id);
  void HardlockPod(const AccountId& account_id);
  void RecordClickOnLockIcon(const AccountId& account_id);
  void OnFocusPod(const AccountId& account_id);
  void OnNoPodFocused();
  void LoadWallpaper(const AccountId& account_id);
  void SignOutUser();
  void CancelAddUser();
  void OnMaxIncorrectPasswordAttempted(const AccountId& account_id);
  void FocusLockScreenApps(bool reverse);

  // Methods to manage lock screen apps focus observers.
  // The observers will be notified when lock screen apps focus changes are
  // reported via lock screen mojo interface.
  void AddLockScreenAppsFocusObserver(LockScreenAppsFocusObserver* observer);
  void RemoveLockScreenAppsFocusObserver(LockScreenAppsFocusObserver* observer);

  // Flushes the mojo pipes - to be used in tests.
  void FlushForTesting();

 private:
  using PendingAuthenticateUserCall =
      base::OnceCallback<void(const std::string& system_salt)>;

  void DoAuthenticateUser(
      const AccountId& account_id,
      const std::string& password,
      bool authenticated_by_pin,
      mojom::LockScreenClient::AuthenticateUserCallback callback,
      const std::string& system_salt);

  void OnGetSystemSalt(const std::string& system_salt);

  // Returns the active data dispatcher or nullptr if there is no lock screen.
  LoginDataDispatcher* DataDispatcher() const;

  // Client interface in chrome browser. May be null in tests.
  mojom::LockScreenClientPtr lock_screen_client_;

  // Bindings for the LockScreen interface.
  mojo::BindingSet<mojom::LockScreen> bindings_;

  // User authentication call that will run when we have system salt.
  PendingAuthenticateUserCall pending_user_auth_;

  base::ObserverList<LockScreenAppsFocusObserver>
      lock_screen_apps_focus_observers_;

  DISALLOW_COPY_AND_ASSIGN(LockScreenController);
};

}  // namespace ash

#endif  // ASH_LOGIN_LOCK_SCREEN_CONTROLLER_H_
