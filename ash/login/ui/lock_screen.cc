// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/lock_screen.h"

#include "ash/login/ui/lock_contents_view.h"
#include "ash/login/ui/lock_window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"

namespace ash {

bool ShowLockScreen() {
  LockWindow* window = new LockWindow();
  window->SetBounds(display::Screen::GetScreen()->GetPrimaryDisplay().bounds());

  views::View* contents = new LockContentsView();
  window->SetContentsView(contents);
  window->Show();

  return true;
}

}  // namespace ash
