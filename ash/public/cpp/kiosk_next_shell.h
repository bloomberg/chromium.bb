// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_KIOSK_NEXT_SHELL_H_
#define ASH_PUBLIC_CPP_KIOSK_NEXT_SHELL_H_

#include "ash/public/cpp/ash_public_export.h"

class AccountId;

namespace ash {

// Performs browser-side functionality for Kiosk Next, e.g. launching the
// KioskNextShell.
class ASH_PUBLIC_EXPORT KioskNextShellClient {
 public:
  // Launch the Kiosk Next Shell for the user identified by |account_id|.
  virtual void LaunchKioskNextShell(const AccountId& account_id) = 0;

 protected:
  virtual ~KioskNextShellClient() = default;
};

// Interface that allows Chrome to notify Ash when the KioskNextShellClient is
// ready.
class ASH_PUBLIC_EXPORT KioskNextShellController {
 public:
  static KioskNextShellController* Get();

  // Registers the client, and if non-null, launches the Kiosk Next Shell
  // session.
  virtual void SetClientAndLaunchSession(KioskNextShellClient* client) = 0;

 protected:
  KioskNextShellController();
  virtual ~KioskNextShellController();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_KIOSK_NEXT_SHELL_H_
