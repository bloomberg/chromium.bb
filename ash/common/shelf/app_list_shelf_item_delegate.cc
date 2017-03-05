// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/app_list_shelf_item_delegate.h"

#include "ash/common/shelf/shelf_model.h"
#include "ash/common/wm_shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/memory/ptr_util.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

// static
void AppListShelfItemDelegate::CreateAppListItemAndDelegate(ShelfModel* model) {
  // Add the app list item to the shelf model.
  ShelfItem item;
  item.type = TYPE_APP_LIST;
  item.title = l10n_util::GetStringUTF16(IDS_ASH_SHELF_APP_LIST_LAUNCHER_TITLE);
  int index = model->Add(item);
  DCHECK_GE(index, 0);

  // Create an AppListShelfItemDelegate for that item.
  ShelfID id = model->items()[index].id;
  DCHECK_GE(id, 0);
  model->SetShelfItemDelegate(id, base::MakeUnique<AppListShelfItemDelegate>());
}

AppListShelfItemDelegate::AppListShelfItemDelegate() {}

AppListShelfItemDelegate::~AppListShelfItemDelegate() {}

ShelfAction AppListShelfItemDelegate::ItemSelected(ui::EventType event_type,
                                                   int event_flags,
                                                   int64_t display_id,
                                                   ShelfLaunchSource source) {
  WmShell::Get()->ToggleAppList();
  return SHELF_ACTION_APP_LIST_SHOWN;
}

ShelfAppMenuItemList AppListShelfItemDelegate::GetAppMenuItems(
    int event_flags) {
  // Return an empty item list to avoid showing an application menu.
  return ShelfAppMenuItemList();
}

void AppListShelfItemDelegate::ExecuteCommand(uint32_t command_id,
                                              int event_flags) {
  // This delegate does not support showing an application menu.
  NOTIMPLEMENTED();
}

void AppListShelfItemDelegate::Close() {}

}  // namespace ash
