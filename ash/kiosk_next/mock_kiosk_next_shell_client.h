// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KIOSK_NEXT_MOCK_KIOSK_NEXT_SHELL_CLIENT_H_
#define ASH_KIOSK_NEXT_MOCK_KIOSK_NEXT_SHELL_CLIENT_H_

#include "ash/public/cpp/kiosk_next_shell.h"
#include "base/macros.h"
#include "components/account_id/account_id.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockKioskNextShellClient : public KioskNextShellClient {
 public:
  MockKioskNextShellClient();
  ~MockKioskNextShellClient() override;

  // KioskNextShellClient:
  MOCK_METHOD1(LaunchKioskNextShell, void(const AccountId& account_id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockKioskNextShellClient);
};

}  // namespace ash

#endif  // ASH_KIOSK_NEXT_MOCK_KIOSK_NEXT_SHELL_CLIENT_H_
