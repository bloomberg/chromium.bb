// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/kiosk_next/mock_kiosk_next_shell_client.h"

namespace ash {

MockKioskNextShellClient::MockKioskNextShellClient() {
  KioskNextShellController::Get()->SetClientAndLaunchSession(this);
}

MockKioskNextShellClient::~MockKioskNextShellClient() {
  KioskNextShellController::Get()->SetClientAndLaunchSession(nullptr);
}

}  // namespace ash
