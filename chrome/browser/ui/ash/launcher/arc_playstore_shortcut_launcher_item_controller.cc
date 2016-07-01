// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_auth_service.h"
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
                                        controller) {}

ArcPlaystoreShortcutLauncherItemController::
    ~ArcPlaystoreShortcutLauncherItemController() {}

ash::ShelfItemDelegate::PerformedAction
ArcPlaystoreShortcutLauncherItemController::Activate(ash::LaunchSource source) {
  arc::ArcAuthService* auth_service = arc::ArcAuthService::Get();
  ArcAppListPrefs* arc_app_prefs =
      ArcAppListPrefs::Get(controller()->GetProfile());
  DCHECK(auth_service);
  DCHECK(arc_app_prefs);
  DCHECK(auth_service->IsAllowed());

  auth_service->EnableArc();

  // Deferred launcher.
  if (arc_app_prefs->IsRegistered(arc::kPlayStoreAppId)) {
    // Known apps can be launched directly or deferred.
    arc::LaunchApp(controller()->GetProfile(), arc::kPlayStoreAppId, true);
  } else {
    // Launch Play Store once its app appears.
    playstore_launcher_.reset(new ArcAppLauncher(controller()->GetProfile(),
                                                 arc::kPlayStoreAppId, true));
  }

  return kNoAction;
}
