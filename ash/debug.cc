// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/debug.h"

#include "ash/shell.h"
#include "ui/aura/root_window.h"
#include "ui/compositor/compositor.h"

namespace ash {
namespace debug {

void ToggleShowPaintRects() {
  Shell::RootWindowList root_windows =
      Shell::GetInstance()->GetAllRootWindows();
  scoped_ptr<bool> value;
  for (Shell::RootWindowList::iterator it = root_windows.begin();
       it != root_windows.end(); ++it) {
    ui::Compositor* compositor = (*it)->compositor();
    if (!value.get())
      value.reset(new bool(!compositor->IsShowPaintRectsEnabled()));
    compositor->SetShowPaintRectsEnabled(*value.get());
  }
}

}  // debug
}  // ash
