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

namespace {
// Reference to global lock screen instance. There can only ever be one lock
// screen display at the same time.
LockWindow* g_window = nullptr;
}  // namespace

bool ShowLockScreen() {
  CHECK(!g_window);
  g_window = new LockWindow();
  g_window->SetBounds(
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds());

  auto* contents = new LockContentsView();
  g_window->SetContentsView(contents);
  g_window->Show();

  return true;
}

void DestroyLockScreen() {
  CHECK(g_window);
  g_window->Close();
  g_window = nullptr;
}

}  // namespace ash
