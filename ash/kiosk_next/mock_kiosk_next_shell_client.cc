// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/kiosk_next/mock_kiosk_next_shell_client.h"

#include "ash/kiosk_next/kiosk_next_shell_controller.h"
#include "ash/shell.h"

namespace ash {

MockKioskNextShellClient::MockKioskNextShellClient() = default;

MockKioskNextShellClient::~MockKioskNextShellClient() = default;

mojom::KioskNextShellClientPtr
MockKioskNextShellClient::CreateInterfacePtrAndBind() {
  mojom::KioskNextShellClientPtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  return ptr;
}

std::unique_ptr<MockKioskNextShellClient> BindMockKioskNextShellClient() {
  auto client = std::make_unique<MockKioskNextShellClient>();
  Shell::Get()->kiosk_next_shell_controller()->SetClient(
      client->CreateInterfacePtrAndBind());
  return client;
}

}  // namespace ash
