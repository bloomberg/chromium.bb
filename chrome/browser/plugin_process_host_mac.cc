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
#include "chrome/common/plugin_messages.h"


void PluginProcessHost::OnPluginSelectWindow(uint32 window_id,
                                             gfx::Rect window_rect,
                                             bool modal) {
  plugin_visible_windows_set_.insert(window_id);
  if (modal)
    plugin_modal_windows_set_.insert(window_id);
}

void PluginProcessHost::OnPluginShowWindow(uint32 window_id,
                                           gfx::Rect window_rect,
                                           bool modal) {
  plugin_visible_windows_set_.insert(window_id);
  if (modal)
    plugin_modal_windows_set_.insert(window_id);
  CGRect window_bounds = {
    { window_rect.x(), window_rect.y() },
    { window_rect.width(), window_rect.height() }
  };
  CGRect main_display_bounds = CGDisplayBounds(CGMainDisplayID());
  if (CGRectEqualToRect(window_bounds, main_display_bounds) &&
      (plugin_fullscreen_windows_set_.find(window_id) ==
       plugin_fullscreen_windows_set_.end())) {
    plugin_fullscreen_windows_set_.insert(window_id);
    // If the plugin has just shown a window that's the same dimensions as
    // the main display, hide the menubar so that it has the whole screen.
    // (but only if we haven't already seen this fullscreen window, since
    // otherwise our refcounting can get skewed).
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableFunction(mac_util::RequestFullScreen));
  }
}

void PluginProcessHost::OnPluginHideWindow(uint32 window_id,
                                           gfx::Rect window_rect) {
  plugin_visible_windows_set_.erase(window_id);
  plugin_modal_windows_set_.erase(window_id);
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

void PluginProcessHost::OnAppActivation() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  // If our plugin process has any modal windows up, we need to bring it forward
  // so that they act more like an in-process modal window would.
  if (!plugin_modal_windows_set_.empty()) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableFunction(mac_util::ActivateProcess, handle()));
  }
}

void PluginProcessHost::OnPluginReceivedFocus(int process_id, int instance_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  // A plugin has received keyboard focus, so tell all other plugin processes
  // that they no longer have it (simulating the OS-level focus notifications
  // that Gtk and Windows provide).
  for (ChildProcessHost::Iterator iter(ChildProcessInfo::PLUGIN_PROCESS);
       !iter.Done(); ++iter) {
    PluginProcessHost* plugin = static_cast<PluginProcessHost*>(*iter);
    int instance = (plugin->handle() == process_id) ? instance_id : 0;
    plugin->Send(new PluginProcessMsg_PluginFocusNotify(instance));
  }
}
