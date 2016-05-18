// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_auth_service.h"
#include "chrome/browser/chromeos/arc/arc_support_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_launcher.h"
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
  DCHECK(auth_service->IsAllowed());

  if (!auth_service->IsArcEnabled())
    auth_service->EnableArc();

  // Deferred launcher.
  playstore_launcher_.reset(
      new ArcAppLauncher(controller()->profile(), arc::kPlayStoreAppId, true));

  return kNoAction;
}
