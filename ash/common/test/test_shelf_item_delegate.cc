// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/test/test_shelf_item_delegate.h"

#include "ash/common/wm_window.h"

namespace ash {
namespace test {

TestShelfItemDelegate::TestShelfItemDelegate(WmWindow* window)
    : window_(window) {}

TestShelfItemDelegate::~TestShelfItemDelegate() {}

ShelfItemDelegate::PerformedAction TestShelfItemDelegate::ItemSelected(
    const ui::Event& event) {
  if (window_) {
    if (window_->GetType() == ui::wm::WINDOW_TYPE_PANEL)
      window_->MoveToEventRoot(event);
    window_->Show();
    window_->Activate();
    return kExistingWindowActivated;
  }
  return kNoAction;
}

ShelfMenuModel* TestShelfItemDelegate::CreateApplicationMenu(int event_flags) {
  return nullptr;
}

void TestShelfItemDelegate::Close() {}

}  // namespace test
}  // namespace ash
