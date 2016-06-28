// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_item_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/ash/launcher/arc_app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_app_menu_item_v2app.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_application_menu_item_model.h"

ArcAppWindowLauncherItemController::ArcAppWindowLauncherItemController(
    const std::string& arc_app_id,
    ChromeLauncherController* controller)
    : AppWindowLauncherItemController(TYPE_APP,
                                      arc_app_id,
                                      arc_app_id,
                                      controller) {}

ArcAppWindowLauncherItemController::~ArcAppWindowLauncherItemController() {}

base::string16 ArcAppWindowLauncherItemController::GetTitle() {
  ArcAppListPrefs* arc_prefs =
      ArcAppListPrefs::Get(launcher_controller()->GetProfile());
  DCHECK(arc_prefs);
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = arc_prefs->GetApp(
      ArcAppWindowLauncherController::GetArcAppIdFromShelfAppId(app_id()));
  if (!app_info) {
    NOTREACHED();
    return base::string16();
  }

  return base::UTF8ToUTF16(app_info->name);
}

ash::ShelfMenuModel* ArcAppWindowLauncherItemController::CreateApplicationMenu(
    int event_flags) {
  return new LauncherApplicationMenuItemModel(GetApplicationList(event_flags));
}

ChromeLauncherAppMenuItems
ArcAppWindowLauncherItemController::GetApplicationList(int event_flags) {
  ChromeLauncherAppMenuItems items =
      AppWindowLauncherItemController::GetApplicationList(event_flags);
  for (size_t i = 0; i < windows().size(); ++i) {
    // TODO(khmel): resolve correct icon here.
    gfx::Image image;
    items.push_back(new ChromeLauncherAppMenuItemV2App(
        GetTitle(), &image, app_id(), launcher_controller(), i,
        i == 0 /* has_leading_separator */));
  }
  return items;
}
