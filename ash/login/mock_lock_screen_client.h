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

  // mojom::LockScreenClient:
  MOCK_METHOD3(AuthenticateUser,
               void(const AccountId& account_id,
                    const std::string& password,
                    bool authenticated_by_pin));
  MOCK_METHOD1(AttemptUnlock, void(const AccountId& account_id));
  MOCK_METHOD1(HardlockPod, void(const AccountId& account_id));
  MOCK_METHOD1(RecordClickOnLockIcon, void(const AccountId& account_id));

 private:
  mojo::Binding<ash::mojom::LockScreenClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(MockLockScreenClient);
};

// Helper method to bind a lock screen client so it receives all mojo calls.
std::unique_ptr<MockLockScreenClient> BindMockLockScreenClient();

}  // namespace ash

#endif  // ASH_LOGIN_MOCK_LOCK_SCREEN_CLIENT_H_