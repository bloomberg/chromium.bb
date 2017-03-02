// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/test/test_shelf_item_delegate.h"

#include "ash/common/wm_window.h"
#include "ash/wm/window_util.h"

namespace ash {
namespace test {

TestShelfItemDelegate::TestShelfItemDelegate(WmWindow* window)
    : window_(window) {}

TestShelfItemDelegate::~TestShelfItemDelegate() {}

ShelfAction TestShelfItemDelegate::ItemSelected(ui::EventType event_type,
                                                int event_flags,
                                                int64_t display_id,
                                                ShelfLaunchSource source) {
  if (window_) {
    if (window_->GetType() == ui::wm::WINDOW_TYPE_PANEL)
      wm::MoveWindowToDisplay(window_->aura_window(), display_id);
    window_->Show();
    window_->Activate();
    return SHELF_ACTION_WINDOW_ACTIVATED;
  }
  return SHELF_ACTION_NONE;
}

ShelfAppMenuItemList TestShelfItemDelegate::GetAppMenuItems(int event_flags) {
  // Return an empty item list to avoid showing an application menu.
  return ShelfAppMenuItemList();
}

void TestShelfItemDelegate::ExecuteCommand(uint32_t command_id,
                                           int event_flags) {
  // This delegate does not support showing an application menu.
  NOTIMPLEMENTED();
}

void TestShelfItemDelegate::Close() {}

}  // namespace test
}  // namespace ash
