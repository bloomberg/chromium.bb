// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

#include "ash/public/interfaces/constants.mojom.h"
#include "base/auto_reset.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/extensions/extension_app_icon_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon_loader.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/browser/ui/ash/launcher/launcher_controller_helper.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

// static
ChromeLauncherController* ChromeLauncherController::instance_ = nullptr;

ChromeLauncherController::~ChromeLauncherController() {
  if (instance_ == this)
    instance_ = nullptr;
}

void ChromeLauncherController::Init() {
  // Start observing the shelf controller.
  if (ConnectToShelfController()) {
    ash::mojom::ShelfObserverAssociatedPtrInfo ptr_info;
    observer_binding_.Bind(&ptr_info);
    shelf_controller_->AddObserver(std::move(ptr_info));
  }
  OnInit();
}

void ChromeLauncherController::LaunchApp(ash::AppLauncherId id,
                                         ash::ShelfLaunchSource source,
                                         int event_flags) {
  launcher_controller_helper_->LaunchApp(id, source, event_flags);
}

ChromeLauncherController::ChromeLauncherController() : observer_binding_(this) {
  DCHECK(!instance_);
  instance_ = this;
}

bool ChromeLauncherController::ConnectToShelfController() {
  if (shelf_controller_.is_bound())
    return true;

  auto* connection = content::ServiceManagerConnection::GetForProcess();
  auto* connector = connection ? connection->GetConnector() : nullptr;
  // Unit tests may not have a connector.
  if (!connector)
    return false;

  connector->BindInterface(ash::mojom::kServiceName, &shelf_controller_);
  return true;
}

AppIconLoader* ChromeLauncherController::GetAppIconLoaderForApp(
    const std::string& app_id) {
  for (const auto& app_icon_loader : app_icon_loaders_) {
    if (app_icon_loader->CanLoadImageForApp(app_id))
      return app_icon_loader.get();
  }

  return nullptr;
}

void ChromeLauncherController::SetShelfAutoHideBehaviorFromPrefs() {
  if (!ConnectToShelfController() || updating_shelf_pref_from_observer_)
    return;

  // The pref helper functions return default values for invalid display ids.
  PrefService* prefs = profile_->GetPrefs();
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    shelf_controller_->SetAutoHideBehavior(
        ash::launcher::GetShelfAutoHideBehaviorPref(prefs, display.id()),
        display.id());
  }
}

void ChromeLauncherController::SetShelfAlignmentFromPrefs() {
  if (!ConnectToShelfController() || updating_shelf_pref_from_observer_)
    return;

  // The pref helper functions return default values for invalid display ids.
  PrefService* prefs = profile_->GetPrefs();
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    shelf_controller_->SetAlignment(
        ash::launcher::GetShelfAlignmentPref(prefs, display.id()),
        display.id());
  }
}

void ChromeLauncherController::SetShelfBehaviorsFromPrefs() {
  SetShelfAutoHideBehaviorFromPrefs();
  SetShelfAlignmentFromPrefs();
}

void ChromeLauncherController::SetLauncherControllerHelperForTest(
    std::unique_ptr<LauncherControllerHelper> helper) {
  launcher_controller_helper_ = std::move(helper);
}

void ChromeLauncherController::SetAppIconLoadersForTest(
    std::vector<std::unique_ptr<AppIconLoader>>& loaders) {
  app_icon_loaders_.clear();
  for (auto& loader : loaders)
    app_icon_loaders_.push_back(std::move(loader));
}

void ChromeLauncherController::SetProfileForTest(Profile* profile) {
  profile_ = profile;
}

void ChromeLauncherController::AttachProfile(Profile* profile_to_attach) {
  profile_ = profile_to_attach;
  // Either add the profile to the list of known profiles and make it the active
  // one for some functions of LauncherControllerHelper or create a new one.
  if (!launcher_controller_helper_.get()) {
    launcher_controller_helper_ =
        base::MakeUnique<LauncherControllerHelper>(profile_);
  } else {
    launcher_controller_helper_->set_profile(profile_);
  }

  // TODO(skuhne): The AppIconLoaderImpl has the same problem. Each loaded
  // image is associated with a profile (its loader requires the profile).
  // Since icon size changes are possible, the icon could be requested to be
  // reloaded. However - having it not multi profile aware would cause problems
  // if the icon cache gets deleted upon user switch.
  std::unique_ptr<AppIconLoader> extension_app_icon_loader =
      base::MakeUnique<extensions::ExtensionAppIconLoader>(
          profile_, extension_misc::EXTENSION_ICON_SMALL, this);
  app_icon_loaders_.push_back(std::move(extension_app_icon_loader));

  if (arc::IsArcAllowedForProfile(profile_)) {
    std::unique_ptr<AppIconLoader> arc_app_icon_loader =
        base::MakeUnique<ArcAppIconLoader>(
            profile_, extension_misc::EXTENSION_ICON_SMALL, this);
    app_icon_loaders_.push_back(std::move(arc_app_icon_loader));
  }

  SetShelfBehaviorsFromPrefs();
}

void ChromeLauncherController::OnShelfCreated(int64_t display_id) {
  if (!ConnectToShelfController())
    return;

  // The pref helper functions return default values for invalid display ids.
  PrefService* prefs = profile_->GetPrefs();
  shelf_controller_->SetAlignment(
      ash::launcher::GetShelfAlignmentPref(prefs, display_id), display_id);
  shelf_controller_->SetAutoHideBehavior(
      ash::launcher::GetShelfAutoHideBehaviorPref(prefs, display_id),
      display_id);
}

void ChromeLauncherController::OnAlignmentChanged(ash::ShelfAlignment alignment,
                                                  int64_t display_id) {
  // The locked alignment is set temporarily and not saved to preferences.
  if (alignment == ash::SHELF_ALIGNMENT_BOTTOM_LOCKED)
    return;
  DCHECK(!updating_shelf_pref_from_observer_);
  base::AutoReset<bool> updating(&updating_shelf_pref_from_observer_, true);
  // This will uselessly store a preference value for invalid display ids.
  ash::launcher::SetShelfAlignmentPref(profile_->GetPrefs(), display_id,
                                       alignment);
}

void ChromeLauncherController::OnAutoHideBehaviorChanged(
    ash::ShelfAutoHideBehavior auto_hide,
    int64_t display_id) {
  DCHECK(!updating_shelf_pref_from_observer_);
  base::AutoReset<bool> updating(&updating_shelf_pref_from_observer_, true);
  // This will uselessly store a preference value for invalid display ids.
  ash::launcher::SetShelfAutoHideBehaviorPref(profile_->GetPrefs(), display_id,
                                              auto_hide);
}

void ChromeLauncherController::OnAppImageUpdated(const std::string& app_id,
                                                 const gfx::ImageSkia& image) {
  // Implemented by subclasses; this should not be called.
  NOTREACHED();
}
