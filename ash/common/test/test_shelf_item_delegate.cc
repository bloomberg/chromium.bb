// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/test/test_shelf_item_delegate.h"

#include "ash/common/wm_window.h"

namespace ash {
namespace test {

TestShelfItemDelegate::TestShelfItemDelegate(WmWindow* window)
    : window_(window), is_draggable_(true) {}

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

base::string16 TestShelfItemDelegate::GetTitle() {
  return window_ ? window_->GetTitle() : base::string16();
}

ShelfMenuModel* TestShelfItemDelegate::CreateApplicationMenu(int event_flags) {
  return nullptr;
}

bool TestShelfItemDelegate::IsDraggable() {
  return is_draggable_;
}

bool TestShelfItemDelegate::CanPin() const {
  return true;
}

bool TestShelfItemDelegate::ShouldShowTooltip() {
  return true;
}

void TestShelfItemDelegate::Close() {}

}  // namespace test
}  // namespace ash
