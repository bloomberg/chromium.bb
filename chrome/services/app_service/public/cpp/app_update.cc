// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/public/cpp/app_update.h"

#include "base/logging.h"
#include "base/time/time.h"

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
  if (delta->short_name.has_value()) {
    state->short_name = delta->short_name;
  }
  if (!delta->icon_key.is_null()) {
    state->icon_key = delta->icon_key.Clone();
  }
  if (delta->last_launch_time.has_value()) {
    state->last_launch_time = delta->last_launch_time;
  }
  if (delta->install_time.has_value()) {
    state->install_time = delta->install_time;
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
  if (delta->is_platform_app != apps::mojom::OptionalBool::kUnknown) {
    state->is_platform_app = delta->is_platform_app;
  }
  if (delta->show_in_launcher != apps::mojom::OptionalBool::kUnknown) {
    state->show_in_launcher = delta->show_in_launcher;
  }
  if (delta->show_in_search != apps::mojom::OptionalBool::kUnknown) {
    state->show_in_search = delta->show_in_search;
  }
  if (delta->show_in_management != apps::mojom::OptionalBool::kUnknown) {
    state->show_in_management = delta->show_in_management;
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

const std::string& AppUpdate::ShortName() const {
  if (delta_ && delta_->short_name.has_value()) {
    return delta_->short_name.value();
  }
  if (state_ && state_->short_name.has_value()) {
    return state_->short_name.value();
  }
  return base::EmptyString();
}

bool AppUpdate::ShortNameChanged() const {
  return delta_ && delta_->short_name.has_value() &&
         (!state_ || (delta_->short_name != state_->short_name));
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

base::Time AppUpdate::LastLaunchTime() const {
  if (delta_ && delta_->last_launch_time.has_value()) {
    return delta_->last_launch_time.value();
  }
  if (state_ && state_->last_launch_time.has_value()) {
    return state_->last_launch_time.value();
  }
  return base::Time();
}

bool AppUpdate::LastLaunchTimeChanged() const {
  return delta_ && delta_->last_launch_time.has_value() &&
         (!state_ || (delta_->last_launch_time != state_->last_launch_time));
}

base::Time AppUpdate::InstallTime() const {
  if (delta_ && delta_->install_time.has_value()) {
    return delta_->install_time.value();
  }
  if (state_ && state_->install_time.has_value()) {
    return state_->install_time.value();
  }
  return base::Time();
}

bool AppUpdate::InstallTimeChanged() const {
  return delta_ && delta_->install_time.has_value() &&
         (!state_ || (delta_->install_time != state_->install_time));
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

apps::mojom::OptionalBool AppUpdate::IsPlatformApp() const {
  if (delta_ &&
      (delta_->is_platform_app != apps::mojom::OptionalBool::kUnknown)) {
    return delta_->is_platform_app;
  }
  if (state_) {
    return state_->is_platform_app;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool AppUpdate::IsPlatformAppChanged() const {
  return delta_ &&
         (delta_->is_platform_app != apps::mojom::OptionalBool::kUnknown) &&
         (!state_ || (delta_->is_platform_app != state_->is_platform_app));
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

apps::mojom::OptionalBool AppUpdate::ShowInManagement() const {
  if (delta_ &&
      (delta_->show_in_management != apps::mojom::OptionalBool::kUnknown)) {
    return delta_->show_in_management;
  }
  if (state_) {
    return state_->show_in_management;
  }
  return apps::mojom::OptionalBool::kUnknown;
}

bool AppUpdate::ShowInManagementChanged() const {
  return delta_ &&
         (delta_->show_in_management != apps::mojom::OptionalBool::kUnknown) &&
         (!state_ ||
          (delta_->show_in_management != state_->show_in_management));
}

}  // namespace apps
