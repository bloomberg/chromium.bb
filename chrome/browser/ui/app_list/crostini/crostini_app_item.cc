// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/crostini/crostini_app_item.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/crostini/crostini_app_context_menu.h"
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

void CrostiniAppItem::Activate(int event_flags) {
  LaunchCrostiniApp(profile(), id());
  // TODO(timloh): Detect and do something if launching failed, as otherwise
  // the app launcher remains open and there's no feedback.
}

ui::MenuModel* CrostiniAppItem::GetContextMenuModel() {
  context_menu_.reset(
      new CrostiniAppContextMenu(profile(), id(), GetController()));
  return context_menu_->GetMenuModel();
}

app_list::AppContextMenu* CrostiniAppItem::GetAppContextMenu() {
  return context_menu_.get();
}
