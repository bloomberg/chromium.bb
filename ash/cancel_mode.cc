// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/cancel_mode.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ui/aura/root_window.h"

namespace ash {

void DispatchCancelMode() {
  Shell::RootWindowControllerList controllers(
      Shell::GetAllRootWindowControllers());
  for (Shell::RootWindowControllerList::const_iterator i = controllers.begin();
       i != controllers.end(); ++i) {
    (*i)->root_window()->AsRootWindowHostDelegate()->OnHostCancelMode();
  }
}

}  // namespace ash
