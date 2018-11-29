// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/contained_shell/mock_contained_shell_client.h"

#include "ash/contained_shell/contained_shell_controller.h"
#include "ash/shell.h"

namespace ash {

MockContainedShellClient::MockContainedShellClient() = default;

MockContainedShellClient::~MockContainedShellClient() = default;

mojom::ContainedShellClientPtr
MockContainedShellClient::CreateInterfacePtrAndBind() {
  mojom::ContainedShellClientPtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  return ptr;
}

std::unique_ptr<MockContainedShellClient> BindMockContainedShellClient() {
  auto client = std::make_unique<MockContainedShellClient>();
  Shell::Get()->contained_shell_controller()->SetClient(
      client->CreateInterfacePtrAndBind());
  return client;
}

}  // namespace ash
