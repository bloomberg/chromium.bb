// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/window_finder.h"

#include "ui/views/widget/desktop_aura/x11_topmost_window_finder.h"

#if defined(USE_ASH)
gfx::NativeWindow GetLocalProcessWindowAtPointAsh(
    const gfx::Point& screen_point,
    const std::set<gfx::NativeWindow>& ignore);
#endif

gfx::NativeWindow GetLocalProcessWindowAtPoint(
    chrome::HostDesktopType host_desktop_type,
    const gfx::Point& screen_point,
    const std::set<gfx::NativeWindow>& ignore) {
#if defined(USE_ASH)
  if (host_desktop_type == chrome::HOST_DESKTOP_TYPE_ASH)
    return GetLocalProcessWindowAtPointAsh(screen_point, ignore);
#endif

  // The X11 server is the canonical state of what the window stacking order
  // is.
  views::X11TopmostWindowFinder finder;
  return finder.FindLocalProcessWindowAt(screen_point, ignore);
}
