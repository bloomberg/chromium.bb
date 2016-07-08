// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/window_finder_mus.h"

#include "ui/aura/window.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/mus/window_manager_connection.h"

WindowFinderMus::WindowFinderMus() {}

WindowFinderMus::~WindowFinderMus() {}

gfx::NativeWindow WindowFinderMus::GetLocalProcessWindowAtPoint(
    const gfx::Point& screen_point,
    const std::set<gfx::NativeWindow>& ignore) {
  std::set<ui::Window*> mus_windows =
      views::WindowManagerConnection::Get()->GetRoots();
  // TODO(erg): Needs to deal with stacking order here.

  // For every mus window, look at the associated aura window and see if we're
  // in that.
  for (ui::Window* mus : mus_windows) {
    views::Widget* widget = views::NativeWidgetMus::GetWidgetForWindow(mus);
    if (widget && widget->GetWindowBoundsInScreen().Contains(screen_point)) {
      aura::Window* content_window = widget->GetNativeWindow();

      // If we were instructed to ignore this window, ignore it.
      if (ContainsKey(ignore, content_window))
        continue;

      return content_window;
    }
  }

  return nullptr;
}
