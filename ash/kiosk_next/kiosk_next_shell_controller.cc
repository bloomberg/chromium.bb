// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/kiosk_next/kiosk_next_shell_controller.h"

#include <memory>
#include <utility>

#include "ash/home_screen/home_screen_controller.h"
#include "ash/kiosk_next/kiosk_next_home_controller.h"
#include "ash/kiosk_next/kiosk_next_shell_observer.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

KioskNextShellController::KioskNextShellController() = default;

KioskNextShellController::~KioskNextShellController() = default;

// static
void KioskNextShellController::RegisterProfilePrefs(
    PrefRegistrySimple* registry,
    bool for_test) {
  if (for_test) {
    registry->RegisterBooleanPref(prefs::kKioskNextShellEnabled, false,
                                  PrefRegistry::PUBLIC);
    return;
  }
}

void KioskNextShellController::BindRequest(
    mojom::KioskNextShellControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

bool KioskNextShellController::IsEnabled() {
  return kiosk_next_enabled_;
}

void KioskNextShellController::AddObserver(KioskNextShellObserver* observer) {
  observer_list_.AddObserver(observer);
}

void KioskNextShellController::RemoveObserver(
    KioskNextShellObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void KioskNextShellController::SetClient(
    mojom::KioskNextShellClientPtr client) {
  kiosk_next_shell_client_ = std::move(client);
}

void KioskNextShellController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  bool prev_kiosk_next_enabled = kiosk_next_enabled_;

  kiosk_next_enabled_ =
      base::FeatureList::IsEnabled(features::kKioskNextShell) &&
      pref_service->GetBoolean(prefs::kKioskNextShellEnabled);

  if (!prev_kiosk_next_enabled && kiosk_next_enabled_) {
    // Replace the AppListController with a KioskNextHomeController.
    kiosk_next_home_controller_ = std::make_unique<KioskNextHomeController>();
    Shell::Get()->home_screen_controller()->SetDelegate(
        kiosk_next_home_controller_.get());
    Shell::Get()->RemoveAppListController();

    kiosk_next_shell_client_->LaunchKioskNextShell(Shell::Get()
                                                       ->session_controller()
                                                       ->GetPrimaryUserSession()
                                                       ->user_info.account_id);

    // Notify observers that KioskNextShell has been enabled.
    for (KioskNextShellObserver& observer : observer_list_) {
      observer.OnKioskNextEnabled();
    }
  }
}
}  // namespace ash
