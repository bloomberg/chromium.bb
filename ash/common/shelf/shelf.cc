// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf.h"

#include "ash/common/shelf/shelf_delegate.h"
#include "ash/common/shelf/shelf_view.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"

namespace ash {

Shelf::Shelf(WmShelf* wm_shelf,
             ShelfView* shelf_view,
             ShelfWidget* shelf_widget)
    : wm_shelf_(wm_shelf),
      shelf_widget_(shelf_widget),
      shelf_view_(shelf_view) {
  DCHECK(wm_shelf_);
  DCHECK(shelf_view_);
  DCHECK(shelf_widget_);
}

Shelf::~Shelf() {
  WmShell::Get()->shelf_delegate()->OnShelfDestroyed(wm_shelf_);
}

// static
Shelf* Shelf::ForPrimaryDisplay() {
  return Shelf::ForWindow(WmShell::Get()->GetPrimaryRootWindow());
}

// static
Shelf* Shelf::ForWindow(WmWindow* window) {
  return window->GetRootWindowController()->GetShelf()->shelf();
}

AppListButton* Shelf::GetAppListButton() const {
  return shelf_view_->GetAppListButton();
}

app_list::ApplicationDragAndDropHost* Shelf::GetDragAndDropHostForAppList() {
  return shelf_view_;
}

}  // namespace ash
