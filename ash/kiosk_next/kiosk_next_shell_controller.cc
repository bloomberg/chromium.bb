// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/kiosk_next/kiosk_next_shell_controller.h"

#include <utility>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

KioskNextShellController::KioskNextShellController() = default;

KioskNextShellController::~KioskNextShellController() = default;

// static
void KioskNextShellController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kKioskNextShellEnabled, false,
                                PrefRegistry::PUBLIC);
}

void KioskNextShellController::BindRequest(
    mojom::KioskNextShellControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

bool KioskNextShellController::IsEnabled() {
  if (!base::FeatureList::IsEnabled(features::kKioskNextShell))
    return false;

  PrefService* prefs =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();

  // If we don't have user prefs, this method is being called before the first
  // sign-in happens. In these cases the shell is always disabled.
  if (!prefs)
    return false;

  return prefs->GetBoolean(prefs::kKioskNextShellEnabled);
}

void KioskNextShellController::LaunchKioskNextShellIfEnabled() {
  if (!IsEnabled())
    return;
  kiosk_next_shell_client_->LaunchKioskNextShell(Shell::Get()
                                                     ->session_controller()
                                                     ->GetPrimaryUserSession()
                                                     ->user_info->account_id);
}

void KioskNextShellController::SetClient(
    mojom::KioskNextShellClientPtr client) {
  kiosk_next_shell_client_ = std::move(client);
}

}  // namespace ash
