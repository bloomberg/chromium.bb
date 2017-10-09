// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/mock_lock_screen_client.h"

#include <memory>

#include "ash/login/lock_screen_controller.h"
#include "ash/shell.h"

namespace ash {

MockLockScreenClient::MockLockScreenClient() : binding_(this) {}

MockLockScreenClient::~MockLockScreenClient() = default;

mojom::LockScreenClientPtr MockLockScreenClient::CreateInterfacePtrAndBind() {
  mojom::LockScreenClientPtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  return ptr;
}

void MockLockScreenClient::AuthenticateUser(const AccountId& account_id,
                                            const std::string& password,
                                            bool authenticated_by_pin,
                                            AuthenticateUserCallback callback) {
  AuthenticateUser_(account_id, password, authenticated_by_pin, callback);
  std::move(callback).Run(authenticate_user_callback_result_);
}

std::unique_ptr<MockLockScreenClient> BindMockLockScreenClient() {
  LockScreenController* lock_screen_controller =
      Shell::Get()->lock_screen_controller();
  auto lock_screen_client = std::make_unique<MockLockScreenClient>();
  lock_screen_controller->SetClient(
      lock_screen_client->CreateInterfacePtrAndBind());
  return lock_screen_client;
}

}  // namespace ash
