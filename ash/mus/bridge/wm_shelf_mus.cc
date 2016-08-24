// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/bridge/wm_shelf_mus.h"

#include "ash/common/shelf/shelf.h"
#include "ash/common/shelf/shelf_delegate.h"
#include "ash/common/shelf/shelf_widget.h"
#include "ash/common/shell_window_ids.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"

namespace ash {
namespace mus {

WmShelfMus::WmShelfMus(WmRootWindowController* root_window_controller) {
  DCHECK(root_window_controller);
  WmShell::Get()->CreateShelfDelegate();
  WmWindow* root = root_window_controller->GetWindow();
  shelf_widget_.reset(new ShelfWidget(
      root->GetChildByShellWindowId(kShellWindowId_ShelfContainer),
      root->GetChildByShellWindowId(kShellWindowId_StatusContainer), this));
  shelf_.reset(
      new Shelf(this, shelf_widget_->CreateShelfView(), shelf_widget_.get()));
  shelf_widget_->set_shelf(shelf_.get());
  // Must be initialized before the delegate is notified because the delegate
  // may try to access the WmShelf.
  SetShelf(shelf_.get());
  WmShell::Get()->shelf_delegate()->OnShelfCreated(this);
  WmShell::Get()->NotifyShelfCreatedForRootWindow(root);
  shelf_widget_->PostCreateShelf();
}

WmShelfMus::~WmShelfMus() {
  shelf_widget_.reset();
  WmShelf::ClearShelf();
}

void WmShelfMus::WillDeleteShelfLayoutManager() {
  shelf_widget_->Shutdown();
  WmShelf::WillDeleteShelfLayoutManager();
}

}  // namespace mus
}  // namespace ash
