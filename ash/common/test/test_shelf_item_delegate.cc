// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/test/test_shelf_item_delegate.h"

#include "ash/common/wm_lookup.h"
#include "ash/common/wm_window.h"
#include "ui/events/event.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace test {

namespace {

// Moves |window| to the root window where the |event| occurred.
// Note: This was forked from ash/wm/window_util.h's wm::MoveWindowToEventRoot.
void MoveWindowToEventRoot(WmWindow* window, const ui::Event& event) {
  views::View* target = static_cast<views::View*>(event.target());
  if (!target)
    return;
  WmWindow* target_root =
      WmLookup::Get()->GetWindowForWidget(target->GetWidget())->GetRootWindow();
  if (!target_root || target_root == window->GetRootWindow())
    return;
  WmWindow* window_container = target_root->GetChildByShellWindowId(
      window->GetParent()->GetShellWindowId());
  window_container->AddChild(window);
}

}  // namespace

TestShelfItemDelegate::TestShelfItemDelegate(WmWindow* window)
    : window_(window), is_draggable_(true) {}

TestShelfItemDelegate::~TestShelfItemDelegate() {}

ShelfItemDelegate::PerformedAction TestShelfItemDelegate::ItemSelected(
    const ui::Event& event) {
  if (window_) {
    if (window_->GetType() == ui::wm::WINDOW_TYPE_PANEL)
      MoveWindowToEventRoot(window_, event);
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
