// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KIOSK_NEXT_MOCK_KIOSK_NEXT_SHELL_CLIENT_H_
#define ASH_KIOSK_NEXT_MOCK_KIOSK_NEXT_SHELL_CLIENT_H_

#include <memory>

#include "ash/public/interfaces/kiosk_next_shell.mojom.h"
#include "base/macros.h"
#include "components/account_id/account_id.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockKioskNextShellClient : public mojom::KioskNextShellClient {
 public:
  MockKioskNextShellClient();
  ~MockKioskNextShellClient() override;

  mojom::KioskNextShellClientPtr CreateInterfacePtrAndBind();

  // mojom::KioskNextShellClient:
  MOCK_METHOD1(LaunchKioskNextShell, void(const AccountId& account_id));

 private:
  mojo::Binding<mojom::KioskNextShellClient> binding_{this};

  DISALLOW_COPY_AND_ASSIGN(MockKioskNextShellClient);
};

// Helper function to bind a KioskNextShellClient so that it receives mojo
// calls.
std::unique_ptr<MockKioskNextShellClient> BindMockKioskNextShellClient();

}  // namespace ash

#endif  // ASH_KIOSK_NEXT_MOCK_KIOSK_NEXT_SHELL_CLIENT_H_
