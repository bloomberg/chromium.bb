// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Carbon/Carbon.h>

#include "build/build_config.h"

#include <vector>

#include "base/logging.h"
#include "base/mac_util.h"
#include "chrome/browser/chrome_thread.h"
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
    plugin_fullscreen_windows_set_.insert(window_id);
    // If the plugin has just shown a window that's the same dimensions as
    // the main display, hide the menubar so that it has the whole screen.
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableFunction(mac_util::RequestFullScreen));
  }
}

void PluginProcessHost::OnPluginHideWindow(uint32 window_id,
                                           gfx::Rect window_rect) {
  plugin_visible_windows_set_.erase(window_id);
  if (plugin_fullscreen_windows_set_.find(window_id) !=
      plugin_fullscreen_windows_set_.end()) {
    plugin_fullscreen_windows_set_.erase(window_id);
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableFunction(mac_util::ReleaseFullScreen));
  }
}

void PluginProcessHost::OnPluginDisposeWindow(uint32 window_id,
                                              gfx::Rect window_rect) {
  OnPluginHideWindow(window_id, window_rect);
}
