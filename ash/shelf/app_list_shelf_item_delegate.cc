// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/app_list_shelf_item_delegate.h"

#include <utility>

#include "ash/public/cpp/shelf_model.h"
#include "ash/shell.h"
#include "ui/app_list/app_list_constants.h"

namespace ash {

AppListShelfItemDelegate::AppListShelfItemDelegate()
    : ShelfItemDelegate(ShelfID(kAppListId)) {}

AppListShelfItemDelegate::~AppListShelfItemDelegate() {}

void AppListShelfItemDelegate::ItemSelected(std::unique_ptr<ui::Event> event,
                                            int64_t display_id,
                                            ShelfLaunchSource source,
                                            ItemSelectedCallback callback) {
  Shell::Get()->ToggleAppList(app_list::kShelfButton);
  std::move(callback).Run(SHELF_ACTION_APP_LIST_SHOWN, base::nullopt);
}

void AppListShelfItemDelegate::ExecuteCommand(bool from_context_menu,
                                              int64_t command_id,
                                              int32_t event_flags,
                                              int64_t display_id) {
  // This delegate does not show custom context or application menu items.
  NOTIMPLEMENTED();
}

void AppListShelfItemDelegate::Close() {}

}  // namespace ash
