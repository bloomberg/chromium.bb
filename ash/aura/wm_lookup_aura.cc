// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/aura/wm_lookup_aura.h"

#include "ash/common/wm_layout_manager.h"
#include "ash/common/wm_window.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ui/views/widget/widget.h"

namespace ash {

WmLookupAura::WmLookupAura() {
  WmLookup::Set(this);
}

WmLookupAura::~WmLookupAura() {
  WmLookup::Set(nullptr);
}

RootWindowController* WmLookupAura::GetRootWindowControllerWithDisplayId(
    int64_t id) {
  aura::Window* root_window = Shell::GetInstance()
                                  ->window_tree_host_manager()
                                  ->GetRootWindowForDisplayId(id);
  return root_window ? RootWindowController::ForWindow(root_window) : nullptr;
}

WmWindow* WmLookupAura::GetWindowForWidget(views::Widget* widget) {
  return WmWindow::Get(widget->GetNativeWindow());
}

}  // namespace ash
