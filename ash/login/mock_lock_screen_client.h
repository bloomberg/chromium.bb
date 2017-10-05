// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_MOCK_LOCK_SCREEN_CLIENT_H_
#define ASH_LOGIN_MOCK_LOCK_SCREEN_CLIENT_H_

#include "ash/public/interfaces/lock_screen.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockLockScreenClient : public mojom::LockScreenClient {
 public:
  MockLockScreenClient();
  ~MockLockScreenClient() override;

  mojom::LockScreenClientPtr CreateInterfacePtrAndBind();

  MOCK_METHOD4(AuthenticateUser_,
               void(const AccountId& account_id,
                    const std::string& password,
                    bool authenticated_by_pin,
                    AuthenticateUserCallback& callback));

  // Set the result that should be passed to |callback| in |AuthenticateUser|.
  void set_authenticate_user_callback_result(bool value) {
    authenticate_user_callback_result_ = value;
  }

  // mojom::LockScreenClient:
  void AuthenticateUser(const AccountId& account_id,
                        const std::string& password,
                        bool authenticated_by_pin,
                        AuthenticateUserCallback callback) override;
  MOCK_METHOD1(AttemptUnlock, void(const AccountId& account_id));
  MOCK_METHOD1(HardlockPod, void(const AccountId& account_id));
  MOCK_METHOD1(RecordClickOnLockIcon, void(const AccountId& account_id));
  MOCK_METHOD1(OnFocusPod, void(const AccountId& account_id));
  MOCK_METHOD0(OnNoPodFocused, void());
  MOCK_METHOD1(LoadWallpaper, void(const AccountId& account_id));
  MOCK_METHOD0(SignOutUser, void());
  MOCK_METHOD0(CancelAddUser, void());
  MOCK_METHOD1(OnMaxIncorrectPasswordAttempted,
               void(const AccountId& account_id));
  MOCK_METHOD1(FocusLockScreenApps, void(bool reverse));

 private:
  bool authenticate_user_callback_result_ = true;

  mojo::Binding<mojom::LockScreenClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(MockLockScreenClient);
};

// Helper method to bind a lock screen client so it receives all mojo calls.
std::unique_ptr<MockLockScreenClient> BindMockLockScreenClient();

}  // namespace ash

#endif  // ASH_LOGIN_MOCK_LOCK_SCREEN_CLIENT_H_
