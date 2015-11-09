// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/window_finder_impl.h"

#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer.h"
#include "ui/wm/core/window_util.h"

gfx::NativeWindow GetLocalProcessWindowAtPointImpl(
    const gfx::Point& screen_point,
    const std::set<gfx::NativeWindow>& ignore,
    const std::set<int>& ignore_ids,
    gfx::NativeWindow window) {
  if (ignore.find(window) != ignore.end())
    return nullptr;

  if (!window->IsVisible())
    return nullptr;

  if (ignore_ids.find(window->id()) != ignore_ids.end())
    return nullptr;

  if (window->layer()->type() == ui::LAYER_TEXTURED) {
    // Returns the window that has visible layer and can hit the
    // |screen_point|, because we want to detach the tab as soon as
    // the dragging mouse moved over to the window that can hide the
    // moving tab.
    aura::client::ScreenPositionClient* client =
        aura::client::GetScreenPositionClient(window->GetRootWindow());
    gfx::Point local_point = screen_point;
    client->ConvertPointFromScreen(window, &local_point);
    return window->GetEventHandlerForPoint(local_point) ? window : nullptr;
  }

  for (aura::Window::Windows::const_reverse_iterator i =
           window->children().rbegin();
       i != window->children().rend(); ++i) {
    gfx::NativeWindow result =
        GetLocalProcessWindowAtPointImpl(screen_point, ignore, ignore_ids, *i);
    if (result)
      return result;
  }
  return nullptr;
}
