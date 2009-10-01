// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Carbon/Carbon.h>

#include "build/build_config.h"

#include <vector>

#include "base/logging.h"
#include "chrome/browser/plugin_process_host.h"

void PluginProcessHost::OnPluginSelectWindow(uint32 window_id,
                                             gfx::Rect window_rect) {
  plugin_visible_windows_set_.insert(window_id);
}

void PluginProcessHost::OnPluginShowWindow(uint32 window_id,
                                           gfx::Rect window_rect) {
  plugin_visible_windows_set_.insert(window_id);
  CGRect window_bounds = {
    { window_rect.x(), window_rect.y() },
    { window_rect.width(), window_rect.height() }
  };
  CGRect main_display_bounds = CGDisplayBounds(CGMainDisplayID());
  if (CGRectEqualToRect(window_bounds, main_display_bounds)) {
    // If the plugin has just shown a window that's the same dimensions as
    // the main display, hide the menubar so that it has the whole screen.
    SetSystemUIMode(kUIModeAllSuppressed, kUIOptionAutoShowMenuBar);
  }
}

void PluginProcessHost::OnPluginHideWindow(uint32 window_id,
                                           gfx::Rect window_rect) {
  plugin_visible_windows_set_.erase(window_id);
  SetSystemUIMode(kUIModeNormal, 0);
}

void PluginProcessHost::OnPluginDisposeWindow(uint32 window_id,
                                              gfx::Rect window_rect) {
  plugin_visible_windows_set_.erase(window_id);
}
