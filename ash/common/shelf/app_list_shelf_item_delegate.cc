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
void AppListShelfItemDelegate::CreateAppListItemAndDelegate(
    ShelfModel* shelf_model) {
  // Add the app list item to the shelf model.
  ShelfItem app_list;
  app_list.type = TYPE_APP_LIST;
  int app_list_index = shelf_model->Add(app_list);
  DCHECK_GE(app_list_index, 0);

  // Create an AppListShelfItemDelegate for that item.
  ShelfID app_list_id = shelf_model->items()[app_list_index].id;
  DCHECK_GE(app_list_id, 0);
  shelf_model->SetShelfItemDelegate(
      app_list_id, base::MakeUnique<AppListShelfItemDelegate>());
}

AppListShelfItemDelegate::AppListShelfItemDelegate() {}

AppListShelfItemDelegate::~AppListShelfItemDelegate() {}

ShelfItemDelegate::PerformedAction AppListShelfItemDelegate::ItemSelected(
    const ui::Event& event) {
  WmShell::Get()->ToggleAppList();
  return ShelfItemDelegate::kAppListMenuShown;
}

base::string16 AppListShelfItemDelegate::GetTitle() {
  ShelfModel* model = WmShell::Get()->shelf_model();
  DCHECK(model);
  int title_id;
  title_id = model->status() == ShelfModel::STATUS_LOADING
                 ? IDS_ASH_SHELF_APP_LIST_LAUNCHER_SYNCING_TITLE
                 : IDS_ASH_SHELF_APP_LIST_LAUNCHER_TITLE;
  return l10n_util::GetStringUTF16(title_id);
}

ShelfMenuModel* AppListShelfItemDelegate::CreateApplicationMenu(
    int event_flags) {
  // AppList does not show an application menu.
  return NULL;
}

bool AppListShelfItemDelegate::IsDraggable() {
  return false;
}

bool AppListShelfItemDelegate::CanPin() const {
  return true;
}

bool AppListShelfItemDelegate::ShouldShowTooltip() {
  return true;
}

void AppListShelfItemDelegate::Close() {}

}  // namespace ash
