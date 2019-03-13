// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KIOSK_NEXT_KIOSK_NEXT_SHELL_CONTROLLER_H_
#define ASH_KIOSK_NEXT_KIOSK_NEXT_SHELL_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/interfaces/kiosk_next_shell.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"

class PrefRegistrySimple;

namespace ash {

// KioskNextShellController allows an ash consumer to manage a Kiosk Next
// session. During this session most system functions are disabled and we launch
// a specific app (Kiosk Next Home) that takes the whole screen.
class ASH_EXPORT KioskNextShellController
    : public mojom::KioskNextShellController {
 public:
  KioskNextShellController();
  ~KioskNextShellController() override;

  // Register prefs related to the Kiosk Next Shell.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Binds the mojom::KioskNextShellController interface to this object.
  void BindRequest(mojom::KioskNextShellControllerRequest request);

  // Returns if the Kiosk Next Shell is enabled for the current user. If there's
  // no signed-in user, this returns false.
  bool IsEnabled();

  // Tries to start the Kiosk Next shell by sending a
  // LaunchKioskNextShell command to the KioskNextShellClient. We will only
  // launch if |IsEnabled()| is true, so it's safe to call this every time a
  // successful sign in happens.
  void LaunchKioskNextShellIfEnabled();

  // mojom::KioskNextShellController:
  void SetClient(mojom::KioskNextShellClientPtr client) override;

 private:
  mojom::KioskNextShellClientPtr kiosk_next_shell_client_;
  mojo::BindingSet<mojom::KioskNextShellController> bindings_;

  DISALLOW_COPY_AND_ASSIGN(KioskNextShellController);
};

}  // namespace ash

#endif  // ASH_KIOSK_NEXT_KIOSK_NEXT_SHELL_CONTROLLER_H_
