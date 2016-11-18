// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_launcher.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/arc_playstore_shortcut_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

ArcPlaystoreShortcutLauncherItemController::
    ArcPlaystoreShortcutLauncherItemController(
        ChromeLauncherController* controller)
    : AppShortcutLauncherItemController(ArcSupportHost::kHostAppId,
                                        "",
                                        controller) {}

ArcPlaystoreShortcutLauncherItemController::
    ~ArcPlaystoreShortcutLauncherItemController() {}

ash::ShelfItemDelegate::PerformedAction
ArcPlaystoreShortcutLauncherItemController::Activate(ash::LaunchSource source) {
  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  DCHECK(arc_session_manager);
  DCHECK(arc_session_manager->IsAllowed());

  ArcAppListPrefs* arc_app_prefs =
      ArcAppListPrefs::Get(controller()->profile());
  DCHECK(arc_app_prefs);

  const bool arc_was_enabled = arc_session_manager->IsArcEnabled();
  arc_session_manager->EnableArc();

  // Deferred launcher.
  if (arc_app_prefs->IsRegistered(arc::kPlayStoreAppId) && arc_was_enabled) {
    // Known apps can be launched directly or deferred.
    arc::LaunchApp(controller()->profile(), arc::kPlayStoreAppId, true);
  } else {
    // Launch Play Store once its app appears.
    playstore_launcher_.reset(new ArcAppLauncher(controller()->profile(),
                                                 arc::kPlayStoreAppId, true));
  }

  return kNoAction;
}
