// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KIOSK_NEXT_KIOSK_NEXT_SHELL_CONTROLLER_H_
#define ASH_KIOSK_NEXT_KIOSK_NEXT_SHELL_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/kiosk_next/kiosk_next_shell_observer.h"
#include "ash/public/interfaces/kiosk_next_shell.mojom.h"
#include "ash/session/session_observer.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "mojo/public/cpp/bindings/binding_set.h"

class PrefRegistrySimple;

namespace ash {

class KioskNextHomeController;
class ShelfModel;

// KioskNextShellController allows an ash consumer to manage a Kiosk Next
// session. During this session most system functions are disabled and we launch
// a specific app (Kiosk Next Home) that takes the whole screen.
class ASH_EXPORT KioskNextShellController
    : public mojom::KioskNextShellController,
      public SessionObserver {
 public:
  KioskNextShellController();
  ~KioskNextShellController() override;

  // Register prefs related to the Kiosk Next Shell.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry, bool for_test);

  // Binds the mojom::KioskNextShellController interface to this object.
  void BindRequest(mojom::KioskNextShellControllerRequest request);

  // Returns if the Kiosk Next Shell is enabled for the current user. If there's
  // no signed-in user, this returns false.
  bool IsEnabled();

  void AddObserver(KioskNextShellObserver* observer);
  void RemoveObserver(KioskNextShellObserver* observer);

  // mojom::KioskNextShellController:
  void SetClient(mojom::KioskNextShellClientPtr client) override;

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;

  ShelfModel* shelf_model() { return shelf_model_.get(); }

 private:
  // Launches Kiosk Next if the pref is enabled and the KioskNextShellClient is
  // available.
  void LaunchKioskNextShellIfEnabled();

  mojom::KioskNextShellClientPtr kiosk_next_shell_client_;
  mojo::BindingSet<mojom::KioskNextShellController> bindings_;
  base::ObserverList<KioskNextShellObserver> observer_list_;
  ScopedSessionObserver session_observer_{this};
  bool kiosk_next_enabled_ = false;

  // Controls the KioskNext home screen when the Kiosk Next Shell is enabled.
  std::unique_ptr<KioskNextHomeController> kiosk_next_home_controller_;

  // When KioskNextShell is enabled, only the home button and back button are
  // made visible on the Shelf. KioskNextShellController therefore hosts its own
  // ShelfModel to control the entries visible on the shelf.
  std::unique_ptr<ShelfModel> shelf_model_;

  DISALLOW_COPY_AND_ASSIGN(KioskNextShellController);
};

}  // namespace ash

#endif  // ASH_KIOSK_NEXT_KIOSK_NEXT_SHELL_CONTROLLER_H_
