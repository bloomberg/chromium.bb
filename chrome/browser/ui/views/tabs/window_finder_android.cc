// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/window_finder.h"

#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/views/tabs/window_finder_impl.h"
#include "ui/aura/window.h"

gfx::NativeWindow GetLocalProcessWindowAtPoint(
    chrome::HostDesktopType host_desktop_type,
    const gfx::Point& screen_point,
    const std::set<gfx::NativeWindow>& ignore,
    gfx::NativeWindow source) {
  gfx::NativeWindow root_window = source ? source->GetRootWindow() : nullptr;
  if (!root_window)
    return nullptr;
  return GetLocalProcessWindowAtPointImpl(screen_point, ignore, std::set<int>(),
                                          root_window);
}
