// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_LOCK_SCREEN_CONTROLLER_H_
#define ASH_LOGIN_LOCK_SCREEN_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/interfaces/lock_screen.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace ash {

// LockScreenController implements mojom::LockScreen and wraps the
// mojom::LockScreenClient interface. This lets a consumer of ash provide a
// LockScreenClient, which we will dispatch to if one has been provided to us.
// This could send requests to LockScreenClient and also handle requests from
// LockScreenClient through mojo.
class ASH_EXPORT LockScreenController
    : NON_EXPORTED_BASE(public mojom::LockScreen) {
 public:
  LockScreenController();
  ~LockScreenController() override;

  // Binds the mojom::LockScreen interface to this object.
  void BindRequest(mojom::LockScreenRequest request);

  // mojom::LockScreen:
  void SetClient(mojom::LockScreenClientPtr client) override;
  void ShowErrorMessage(int32_t login_attempts,
                        const std::string& error_text,
                        const std::string& help_link_text,
                        int32_t help_topic_id) override;
  void ClearErrors() override;
  void ShowUserPodCustomIcon(const AccountId& account_id,
                             mojom::UserPodCustomIconOptionsPtr icon) override;
  void HideUserPodCustomIcon(const AccountId& account_id) override;
  void SetAuthType(const AccountId& account_id,
                   mojom::AuthType auth_type,
                   const base::string16& initial_value) override;
  void LoadUsers(std::unique_ptr<base::ListValue> users,
                 bool show_guest) override;

  // Wrappers around the mojom::LockScreenClient interface.
  // Hash the password and send AuthenticateUser request to LockScreenClient.
  // LockScreenClient(chrome) will do the authentication and request to show
  // error messages in the lock screen if auth fails, or request to clear
  // errors if auth succeeds.
  void AuthenticateUser(const AccountId& account_id,
                        const std::string& password,
                        bool authenticated_by_pin);
  void AttemptUnlock(const AccountId& account_id);
  void HardlockPod(const AccountId& account_id);
  void RecordClickOnLockIcon(const AccountId& account_id);

 private:
  void DoAuthenticateUser(const AccountId& account_id,
                          const std::string& password,
                          bool authenticated_by_pin,
                          const std::string& system_salt);

  // Client interface in chrome browser. May be null in tests.
  mojom::LockScreenClientPtr lock_screen_client_;

  // Bindings for the LockScreen interface.
  mojo::BindingSet<mojom::LockScreen> bindings_;

  DISALLOW_COPY_AND_ASSIGN(LockScreenController);
};

}  // namspace ash

#endif  // ASH_LOGIN_LOCK_SCREEN_CONTROLLER_H_
