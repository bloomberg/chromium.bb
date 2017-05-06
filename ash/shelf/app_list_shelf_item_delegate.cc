// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/app_list_shelf_item_delegate.h"

#include "ash/shelf/shelf_model.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/memory/ptr_util.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// An app id for the app list, used to identify the shelf item.
// Generated as crx_file::id_util::GenerateId("org.chromium.applist")
static constexpr char kAppListId[] = "jlfapfmkapbjlfbpjedlinehodkccjee";

}  // namespace

// static
void AppListShelfItemDelegate::CreateAppListItemAndDelegate(ShelfModel* model) {
  // Add the app list item to the shelf model.
  ShelfItem item;
  item.type = TYPE_APP_LIST;
  item.id = ShelfID(kAppListId);
  item.title = l10n_util::GetStringUTF16(IDS_ASH_SHELF_APP_LIST_LAUNCHER_TITLE);
  int index = model->Add(item);
  DCHECK_GE(index, 0);

  // Create an AppListShelfItemDelegate for that item.
  model->SetShelfItemDelegate(item.id,
                              base::MakeUnique<AppListShelfItemDelegate>());
}

AppListShelfItemDelegate::AppListShelfItemDelegate()
    : ShelfItemDelegate(ShelfID(kAppListId)) {}

AppListShelfItemDelegate::~AppListShelfItemDelegate() {}

void AppListShelfItemDelegate::ItemSelected(
    std::unique_ptr<ui::Event> event,
    int64_t display_id,
    ShelfLaunchSource source,
    const ItemSelectedCallback& callback) {
  Shell::Get()->ToggleAppList();
  callback.Run(SHELF_ACTION_APP_LIST_SHOWN, base::nullopt);
}

void AppListShelfItemDelegate::ExecuteCommand(uint32_t command_id,
                                              int32_t event_flags) {
  // This delegate does not support showing an application menu.
  NOTIMPLEMENTED();
}

void AppListShelfItemDelegate::Close() {}

}  // namespace ash
