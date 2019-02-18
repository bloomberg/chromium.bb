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
  // TODO(ltenorio): Also check the ash.contained_shell.enabled pref here when
  // it's available.
  return base::FeatureList::IsEnabled(features::kContainedShell);
}

void ContainedShellController::LaunchContainedShell() {
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
