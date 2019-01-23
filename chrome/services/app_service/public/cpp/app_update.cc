// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/public/cpp/app_update.h"

#include "base/logging.h"

namespace {

void ClonePermissions(const std::vector<apps::mojom::PermissionPtr>& clone_from,
                      std::vector<apps::mojom::PermissionPtr>* clone_to) {
  for (const auto& permission : clone_from) {
    clone_to->push_back(permission->Clone());
  }
}

}  // namespace

namespace apps {

// static
void AppUpdate::Merge(apps::mojom::App* state, const apps::mojom::App* delta) {
  DCHECK(state);
  if (!delta) {
    return;
  }

  if ((delta->app_type != state->app_type) ||
      (delta->app_id != state->app_id)) {
    LOG(ERROR) << "inconsistent (app_type, app_id): (" << delta->app_type
               << ", " << delta->app_id << ") vs (" << state->app_type << ", "
               << state->app_id << ") ";
    DCHECK(false);
    return;
  }

  if (delta->readiness != apps::mojom::Readiness::kUnknown) {
    state->readiness = delta->readiness;
  }
  if (delta->name.has_value()) {
    state->name = delta->name;
  }
  if (!delta->icon_key.is_null()) {
    state->icon_key = delta->icon_key.Clone();
  }
  if (!delta->permissions.empty()) {
    DCHECK(state->permissions.empty() ||
           (delta->permissions.size() == state->permissions.size()));
    state->permissions.clear();
    ClonePermissions(delta->permissions, &state->permissions);
  }
  if (delta->installed_internally != apps::mojom::OptionalBool::kUnknown) {
    state->installed_internally = delta->installed_internally;
  }
  if (delta->show_in_launcher != apps::mojom::OptionalBool::kUnknown) {
    state->show_in_launcher = delta->show_in_launcher;
  }
  if (delta->show_in_search != apps::mojom::OptionalBool::kUnknown) {
    state->show_in_search = delta->show_in_search;
  }

  // When adding new fields to the App Mojo type, this function should also be
  // updated.
}

AppUpdate::AppUpdate(const apps::mojom::App* state,
                     const apps::mojom::App* delta)
    : state_(state), delta_(delta) {
  DCHECK(state_ || delta_);
  if (state_ && delta_) {
    DCHECK(state_->app_type == delta->app_type);
    DCHECK(state_->app_id == delta->app_id);
  }
}

bool AppUpdate::StateIsNull() const {
  return state_ == nullptr;
}

apps::mojom::AppType AppUpdate::AppType() const {
  return delta_ ? delta_->app_type : state_->app_type;
}

const std::string& AppUpdate::AppId() const {
  return delta_ ? delta_->app_id : state_->app_id;
}

apps::mojom::Readiness AppUpdate::Readiness() const {
  if (delta_ && (delta_->readiness != apps::mojom::Readiness::kUnknown)) {
    return delta_->readiness;
  }
  if (state_) {
    return state_->readiness;
  }
  return apps::mojom::Readiness::kUnknown;
}

bool AppUpdate::ReadinessChanged() const {
  return delta_ && (delta_->readiness != apps::mojom::Readiness::kUnknown) &&
         (!state_ || (delta_->readiness != state_->readiness));
}

const std::string& AppUpdate::Name() const {
  if (delta_ && delta_->name.has_value()) {
    return delta_->name.value();
  }
  if (state_ && state_->name.has_value()) {
    return state_->name.value();
  }
  return base::EmptyString();
}

bool AppUpdate::NameChanged() const {
  return delta_ && delta_->name.has_value() &&
         (!state_ || (delta_->name != state_->name));
}

apps::mojom::IconKeyPtr AppUpdate::IconKey() const {
  if (delta_ && !delta_->icon_key.is_null()) {
    return delta_->icon_key.Clone();
  }
  if (state_ && !state_->icon_key.is_null()) {
    return state_->icon_key.Clone();
  }
  return apps::mojom::IconKeyPtr();
}

bool AppUpdate::IconKeyChanged() const {
  return delta_ && !delta_->icon_key.is_null() &&
         (!state_ || !delta_->icon_key.Equals(state_->icon_key));
}

std::vector<apps::mojom::PermissionPtr> AppUpdate::Permissions() const {
  std::vector<apps::mojom::PermissionPtr> permissions;

  if (delta_ && !delta_->permissions.empty()) {
    ClonePermissions(delta_->permissions, &permissions);
  } else if (state_ && !state_->permissions.empty()) {
    ClonePermissions(state_->permissions, &permissions);
  }

  return permissions;
}

bool AppUpdate::PermissionsChanged() const {
  return delta_ && !delta_->permissions.empty() &&
         (!state_ || (delta_->permissions != state_->permissions));
}

apps::mojom::OptionalBool AppUpdate::InstalledInternally() const {
  if (delta_ &&
      (delta_->installed_internally != apps::mojom::OptionalBool::kUnknown)) {
    return delta_->installed_internally;
  }
  if (state_) {
    return state_->installed_internally;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool AppUpdate::InstalledInternallyChanged() const {
  return delta_ &&
         (delta_->installed_internally !=
          apps::mojom::OptionalBool::kUnknown) &&
         (!state_ ||
          (delta_->installed_internally != state_->installed_internally));
}

apps::mojom::OptionalBool AppUpdate::ShowInLauncher() const {
  if (delta_ &&
      (delta_->show_in_launcher != apps::mojom::OptionalBool::kUnknown)) {
    return delta_->show_in_launcher;
  }
  if (state_) {
    return state_->show_in_launcher;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool AppUpdate::ShowInLauncherChanged() const {
  return delta_ &&
         (delta_->show_in_launcher != apps::mojom::OptionalBool::kUnknown) &&
         (!state_ || (delta_->show_in_launcher != state_->show_in_launcher));
}

apps::mojom::OptionalBool AppUpdate::ShowInSearch() const {
  if (delta_ &&
      (delta_->show_in_search != apps::mojom::OptionalBool::kUnknown)) {
    return delta_->show_in_search;
  }
  if (state_) {
    return state_->show_in_search;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool AppUpdate::ShowInSearchChanged() const {
  return delta_ &&
         (delta_->show_in_search != apps::mojom::OptionalBool::kUnknown) &&
         (!state_ || (delta_->show_in_search != state_->show_in_search));
}

}  // namespace apps
