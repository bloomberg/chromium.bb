// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/app_list_shelf_item_delegate.h"

#include "ash/common/shelf/shelf_model.h"
#include "ash/common/wm_shell.h"
#include "base/memory/ptr_util.h"
#include "grit/ash_strings.h"
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

ShelfItemDelegate::PerformedAction AppListShelfItemDelegate::ItemSelected(
    const ui::Event& event) {
  WmShell::Get()->ToggleAppList();
  return ShelfItemDelegate::kAppListMenuShown;
}

ShelfMenuModel* AppListShelfItemDelegate::CreateApplicationMenu(
    int event_flags) {
  // AppList does not show an application menu.
  return NULL;
}

void AppListShelfItemDelegate::Close() {}

}  // namespace ash
