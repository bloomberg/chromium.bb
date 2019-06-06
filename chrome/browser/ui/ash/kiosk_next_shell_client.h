// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_KIOSK_NEXT_SHELL_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_KIOSK_NEXT_SHELL_CLIENT_H_

#include "ash/public/cpp/kiosk_next_shell.h"
#include "base/macros.h"

class KioskNextShellClient : public ash::KioskNextShellClient {
 public:
  KioskNextShellClient();
  ~KioskNextShellClient() override;

  void Init();

  // Returns the singleton KioskNextShellClient instance, if it exists.
  static KioskNextShellClient* Get();

  // ash::KioskNextShellClient:
  void LaunchKioskNextShell(const AccountId& account_id) override;

  bool has_launched() const { return has_launched_; }

 private:
  // True once the KioskNextShell has been launched.
  bool has_launched_ = false;

  DISALLOW_COPY_AND_ASSIGN(KioskNextShellClient);
};

#endif  // CHROME_BROWSER_UI_ASH_KIOSK_NEXT_SHELL_CLIENT_H_
