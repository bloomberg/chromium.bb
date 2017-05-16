// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/arc_playstore_shortcut_launcher_item_controller.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_launcher.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "ui/events/event_constants.h"

ArcPlaystoreShortcutLauncherItemController::
    ArcPlaystoreShortcutLauncherItemController()
    : AppShortcutLauncherItemController(ash::ShelfID(arc::kPlayStoreAppId)) {}

ArcPlaystoreShortcutLauncherItemController::
    ~ArcPlaystoreShortcutLauncherItemController() {}

void ArcPlaystoreShortcutLauncherItemController::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ash::ShelfLaunchSource source,
    ItemSelectedCallback callback) {
  if (!playstore_launcher_) {
    // Play Store launch request has never been scheduled.
    std::unique_ptr<ArcAppLauncher> playstore_launcher =
        base::MakeUnique<ArcAppLauncher>(
            ChromeLauncherController::instance()->profile(),
            arc::kPlayStoreAppId, true /* landscape_layout */,
            true /* deferred_launch_allowed */);
    // ArcAppLauncher may launch Play Store in case it exists already. In this
    // case this instance of ArcPlaystoreShortcutLauncherItemController may be
    // deleted. If Play Store does not exist at this moment, then let
    // |playstore_launcher_| wait until it appears.
    if (!playstore_launcher->app_launched())
      playstore_launcher_ = std::move(playstore_launcher);
  }
  std::move(callback).Run(ash::SHELF_ACTION_NONE, base::nullopt);
}
