// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/wm_shelf.h"

#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"

namespace ash {

// static
WmShelf* WmShelf::ForPrimaryDisplay() {
  return WmShell::Get()
      ->GetPrimaryRootWindow()
      ->GetRootWindowController()
      ->GetShelf();
}

}  // namespace ash
