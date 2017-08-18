// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_item_delegate.h"

#include "ui/base/models/menu_model.h"

namespace ash {

namespace {

// Get a serialized list of mojo MenuItemPtr objects to transport a menu model.
// NOTE: This does not support button items, some separator types, sublabels,
// minor text, dynamic items, label fonts, accelerators, visibility, etc.
MenuItemList GetMenuItemsForMojo(ui::MenuModel* model) {
  MenuItemList items;
  if (!model)
    return items;
  for (int i = 0; i < model->GetItemCount(); ++i) {
    mojom::MenuItemPtr item(mojom::MenuItem::New());
    DCHECK_NE(ui::MenuModel::TYPE_BUTTON_ITEM, model->GetTypeAt(i));
    item->type = model->GetTypeAt(i);
    item->command_id = model->GetCommandIdAt(i);
    item->label = model->GetLabelAt(i);
    item->checked = model->IsItemCheckedAt(i);
    item->enabled = model->IsEnabledAt(i);
    item->radio_group_id = model->GetGroupIdAt(i);
    if (item->type == ui::MenuModel::TYPE_SUBMENU)
      item->submenu = GetMenuItemsForMojo(model->GetSubmenuModelAt(i));
    items.push_back(std::move(item));
  }
  return items;
}

}  // namespace

ShelfItemDelegate::ShelfItemDelegate(const ShelfID& shelf_id)
    : shelf_id_(shelf_id), binding_(this), image_set_by_controller_(false) {}

ShelfItemDelegate::~ShelfItemDelegate() {}

mojom::ShelfItemDelegatePtr ShelfItemDelegate::CreateInterfacePtrAndBind() {
  mojom::ShelfItemDelegatePtr ptr;
  binding_.Bind(mojo::MakeRequest(&ptr));
  return ptr;
}

MenuItemList ShelfItemDelegate::GetAppMenuItems(int event_flags) {
  return MenuItemList();
}

std::unique_ptr<ui::MenuModel> ShelfItemDelegate::GetContextMenu(
    int64_t display_id) {
  // Shelf items do not have any custom context menu entries by default.
  return nullptr;
}

AppWindowLauncherItemController*
ShelfItemDelegate::AsAppWindowLauncherItemController() {
  return nullptr;
}

bool ShelfItemDelegate::ExecuteContextMenuCommand(int64_t command_id,
                                                  int32_t event_flags) {
  DCHECK(context_menu_);
  // Help subclasses execute context menu items, which may be on a sub-menu.
  ui::MenuModel* model = context_menu_.get();
  int index = -1;
  if (!ui::MenuModel::GetModelAndIndexForCommandId(command_id, &model, &index))
    return false;

  model->ActivatedAt(index, event_flags);
  return true;
}

void ShelfItemDelegate::GetContextMenuItems(
    int64_t display_id,
    GetContextMenuItemsCallback callback) {
  context_menu_ = GetContextMenu(display_id);
  std::move(callback).Run(GetMenuItemsForMojo(context_menu_.get()));
}

}  // namespace ash
