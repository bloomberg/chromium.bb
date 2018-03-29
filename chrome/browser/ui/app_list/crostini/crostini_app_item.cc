// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/crostini/crostini_app_item.h"

#include "chrome/browser/ui/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ui/app_list/crostini/crostini_installer_view.h"
#include "ui/gfx/image/image_skia.h"

// static
const char CrostiniAppItem::kItemType[] = "CrostiniAppItem";

CrostiniAppItem::CrostiniAppItem(
    Profile* profile,
    const app_list::AppListSyncableService::SyncItem* sync_item,
    const std::string& id,
    const std::string& name,
    const gfx::ImageSkia* image_skia)
    : ChromeAppListItem(profile, id) {
  SetIcon(*image_skia);
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
  // TODO(813699): launch the app if needed e.g. like
  // chrome/browser/ui/app_list/arc/arc_app_utils.cc
  // if (!crostini::LaunchApp(profile(), id(), event_flags,
  //                    GetController()->GetAppListDisplayId())) {
  //   return;
  // }

  CrostiniInstallerView::Show(this, profile());
}
