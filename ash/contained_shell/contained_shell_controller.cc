// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/contained_shell/contained_shell_controller.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

#include <utility>

namespace ash {

ContainedShellController::ContainedShellController() = default;

ContainedShellController::~ContainedShellController() = default;

// static
void ContainedShellController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kContainedShellEnabled, false,
                                PrefRegistry::PUBLIC);
}

void ContainedShellController::BindRequest(
    mojom::ContainedShellControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

bool ContainedShellController::IsEnabled() {
  if (!base::FeatureList::IsEnabled(features::kContainedShell))
    return false;

  PrefService* prefs =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();

  DCHECK(prefs) << "PrefService should not be null when reading Contained "
                   "Shell pref. This usually happens when calling "
                   "ContainedShellController::IsEnabled() before sign in.";
  return prefs->GetBoolean(prefs::kContainedShellEnabled);
}

void ContainedShellController::LaunchContainedShellIfEnabled() {
  if (!IsEnabled())
    return;

  contained_shell_client_->LaunchContainedShell(Shell::Get()
                                                    ->session_controller()
                                                    ->GetPrimaryUserSession()
                                                    ->user_info->account_id);
}

void ContainedShellController::SetClient(
    mojom::ContainedShellClientPtr client) {
  contained_shell_client_ = std::move(client);
}

}  // namespace ash
