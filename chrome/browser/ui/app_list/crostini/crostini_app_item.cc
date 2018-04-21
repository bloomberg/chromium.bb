// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/crostini/crostini_app_item.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/crostini/crostini_app_context_menu.h"
#include "chrome/browser/ui/app_list/crostini/crostini_installer_view.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

// static
const char CrostiniAppItem::kItemType[] = "CrostiniAppItem";

CrostiniAppItem::CrostiniAppItem(
    Profile* profile,
    const app_list::AppListSyncableService::SyncItem* sync_item,
    const std::string& id,
    const std::string& name,
    const gfx::ImageSkia* image_skia)
    : ChromeAppListItem(profile, id) {
  SetIcon(gfx::ImageSkiaOperations::CreateResizedImage(
      *image_skia, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(app_list::kTileIconSize, app_list::kTileIconSize)));
  SetName(name);
  if (sync_item && sync_item->item_ordinal.IsValid()) {
    UpdateFromSync(sync_item);
  } else {
    SetDefaultPositionIfApplicable();
  }
}

CrostiniAppItem::~CrostiniAppItem() {}

const char* CrostiniAppItem::GetItemType() const {
  return CrostiniAppItem::kItemType;
}

namespace {
void MaybeLaunchTerminal(Profile* profile,
                         crostini::ConciergeClientResult result) {
  if (result == crostini::ConciergeClientResult::SUCCESS) {
    crostini::CrostiniManager::GetInstance()->LaunchContainerTerminal(
        profile, kCrostiniDefaultVmName, kCrostiniDefaultContainerName);
  }
}

void MaybeLaunchContainerAppplication(
    std::unique_ptr<crostini::CrostiniRegistryService::Registration>
        registration,
    crostini::ConciergeClientResult result) {
  if (result == crostini::ConciergeClientResult::SUCCESS) {
    // TODO(timloh): Do something if launching failed, as otherwise the app
    // launcher remains open and there's no feedback.
    crostini::CrostiniManager::GetInstance()->LaunchContainerApplication(
        registration->vm_name, registration->container_name,
        registration->desktop_file_id,
        base::BindOnce([](crostini::ConciergeClientResult result) {}));
  }
}
}  // namespace

void CrostiniAppItem::Activate(int event_flags) {
  crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(profile());
  std::unique_ptr<crostini::CrostiniRegistryService::Registration>
      registration = registry_service->GetRegistration(id());
  auto* crostini_manager = crostini::CrostiniManager::GetInstance();
  if (registration) {
    crostini_manager->RestartCrostini(
        profile(), registration->vm_name, registration->container_name,
        base::BindOnce(&MaybeLaunchContainerAppplication,
                       std::move(registration)));

    return;
  }
  if (!crostini_manager->IsCrosTerminaInstalled()) {
    CrostiniInstallerView::Show(this, profile());
  } else {
    crostini_manager->RestartCrostini(
        profile(), kCrostiniDefaultVmName, kCrostiniDefaultContainerName,
        base::BindOnce(&MaybeLaunchTerminal, base::Unretained(profile())));
  }
}

ui::MenuModel* CrostiniAppItem::GetContextMenuModel() {
  context_menu_.reset(
      new CrostiniAppContextMenu(profile(), id(), GetController()));
  return context_menu_->GetMenuModel();
}

app_list::AppContextMenu* CrostiniAppItem::GetAppContextMenu() {
  return context_menu_.get();
}
