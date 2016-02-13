// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/chrome_app_list_item.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_service.h"
#include "chrome/browser/ui/host_desktop.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_system.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {

AppListControllerDelegate* g_controller_for_test = nullptr;

} // namespace

// static
void ChromeAppListItem::OverrideAppListControllerDelegateForTesting(
    AppListControllerDelegate* controller) {
  g_controller_for_test = controller;
}

// static
gfx::ImageSkia ChromeAppListItem::CreateDisabledIcon(
    const gfx::ImageSkia& icon) {
  const color_utils::HSL shift = {-1, 0, 0.6};
  return gfx::ImageSkiaOperations::CreateHSLShiftedImage(icon, shift);
}

ChromeAppListItem::ChromeAppListItem(Profile* profile,
                                     const std::string& app_id)
    : app_list::AppListItem(app_id),
      profile_(profile)  {
}

ChromeAppListItem::~ChromeAppListItem() {
}

extensions::AppSorting* ChromeAppListItem::GetAppSorting() {
  return extensions::ExtensionSystem::Get(profile())->app_sorting();
}

void ChromeAppListItem::UpdateFromSync(
    const app_list::AppListSyncableService::SyncItem* sync_item) {
  DCHECK(sync_item && sync_item->item_ordinal.IsValid());
  // An existing synced position exists, use that.
  set_position(sync_item->item_ordinal);
  // Only set the name from the sync item if it is empty.
  if (name().empty())
    SetName(sync_item->item_name);
}

AppListControllerDelegate* ChromeAppListItem::GetController() {
  return g_controller_for_test != nullptr
             ? g_controller_for_test
             : AppListService::Get()->GetControllerDelegate();
}
