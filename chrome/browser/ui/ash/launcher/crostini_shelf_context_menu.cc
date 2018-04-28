// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/crostini_shelf_context_menu.h"

#include "ash/public/cpp/shelf_item.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/ui_base_features.h"

CrostiniShelfContextMenu::CrostiniShelfContextMenu(
    ChromeLauncherController* controller,
    const ash::ShelfItem* item,
    int64_t display_id)
    : LauncherContextMenu(controller, item, display_id) {
  Init();
}

CrostiniShelfContextMenu::~CrostiniShelfContextMenu() {}

void CrostiniShelfContextMenu::Init() {
  const crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(
          controller()->profile());
  std::unique_ptr<crostini::CrostiniRegistryService::Registration>
      registration = registry_service->GetRegistration(item().id.app_id);
  if (registration)
    AddPinMenu();

  if (controller()->IsOpen(item().id))
    AddItemWithStringId(MENU_CLOSE, IDS_LAUNCHER_CONTEXT_MENU_CLOSE);
  else
    AddItemWithStringId(MENU_OPEN_NEW, IDS_APP_CONTEXT_MENU_ACTIVATE_ARC);

  if (!features::IsTouchableAppContextMenuEnabled())
    AddSeparator(ui::NORMAL_SEPARATOR);
}
