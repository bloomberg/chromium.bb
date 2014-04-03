// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ash/scoped_target_root_window.h"

#include "ash/shell.h"

namespace ash {

ScopedTargetRootWindow::ScopedTargetRootWindow(
    aura::Window* root_window) {
  Shell::GetInstance()->scoped_target_root_window_ = root_window;
}

ScopedTargetRootWindow::~ScopedTargetRootWindow() {
  Shell::GetInstance()->scoped_target_root_window_ = NULL;
}

}  // namespace ash
