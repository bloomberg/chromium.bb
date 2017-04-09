// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_shelf_item_delegate.h"

#include "ash/wm/window_util.h"
#include "ash/wm_window.h"

namespace ash {
namespace test {

TestShelfItemDelegate::TestShelfItemDelegate(WmWindow* window)
    : ShelfItemDelegate(AppLaunchId()), window_(window) {}

TestShelfItemDelegate::~TestShelfItemDelegate() {}

void TestShelfItemDelegate::ItemSelected(std::unique_ptr<ui::Event> event,
                                         int64_t display_id,
                                         ShelfLaunchSource source,
                                         const ItemSelectedCallback& callback) {
  if (window_) {
    if (window_->GetType() == ui::wm::WINDOW_TYPE_PANEL)
      wm::MoveWindowToDisplay(window_->aura_window(), display_id);
    window_->Show();
    window_->Activate();
    callback.Run(SHELF_ACTION_WINDOW_ACTIVATED, base::nullopt);
    return;
  }
  callback.Run(SHELF_ACTION_NONE, base::nullopt);
}

void TestShelfItemDelegate::ExecuteCommand(uint32_t command_id,
                                           int32_t event_flags) {
  // This delegate does not support showing an application menu.
  NOTIMPLEMENTED();
}

void TestShelfItemDelegate::Close() {}

}  // namespace test
}  // namespace ash
