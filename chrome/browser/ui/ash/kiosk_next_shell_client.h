// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_KIOSK_NEXT_SHELL_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_KIOSK_NEXT_SHELL_CLIENT_H_

#include "ash/public/interfaces/kiosk_next_shell.mojom.h"
#include "base/macros.h"
#include "components/account_id/account_id.h"
#include "mojo/public/cpp/bindings/binding.h"

class KioskNextShellClient : public ash::mojom::KioskNextShellClient {
 public:
  KioskNextShellClient();
  ~KioskNextShellClient() override;

  // mojom::KioskNextShellClient:
  void LaunchKioskNextShell(const AccountId& account_id) override;

 private:
  mojo::Binding<ash::mojom::KioskNextShellClient> binding_{this};

  DISALLOW_COPY_AND_ASSIGN(KioskNextShellClient);
};

#endif  // CHROME_BROWSER_UI_ASH_KIOSK_NEXT_SHELL_CLIENT_H_
