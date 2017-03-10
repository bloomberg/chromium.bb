// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/arc_playstore_shortcut_launcher_item_controller.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_launcher.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

ArcPlaystoreShortcutLauncherItemController::
    ArcPlaystoreShortcutLauncherItemController(
        ChromeLauncherController* controller)
    : AppShortcutLauncherItemController(ArcSupportHost::kHostAppId,
                                        "",
                                        controller) {}

ArcPlaystoreShortcutLauncherItemController::
    ~ArcPlaystoreShortcutLauncherItemController() {}

void ArcPlaystoreShortcutLauncherItemController::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ash::ShelfLaunchSource source,
    const ItemSelectedCallback& callback) {
  Profile* profile = controller()->profile();
  ArcAppListPrefs* arc_app_prefs = ArcAppListPrefs::Get(profile);
  DCHECK(arc_app_prefs);

  const bool play_store_was_enabled =
      arc::IsArcPlayStoreEnabledForProfile(profile);
  arc::SetArcPlayStoreEnabledForProfile(profile, true);

  // Deferred launcher.
  if (arc_app_prefs->IsRegistered(arc::kPlayStoreAppId) &&
      play_store_was_enabled) {
    // Known apps can be launched directly or deferred.
    arc::LaunchApp(profile, arc::kPlayStoreAppId, true);
  } else {
    // Launch Play Store once its app appears.
    playstore_launcher_ =
        base::MakeUnique<ArcAppLauncher>(profile, arc::kPlayStoreAppId, true);
  }

  callback.Run(ash::SHELF_ACTION_NONE,
               GetAppMenuItems(event ? event->flags() : ui::EF_NONE));
}
