// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/window_finder.h"

#include "ash/shell_window_ids.h"
#include "ash/wm/coordinate_conversion.h"
#include "chrome/browser/ui/views/tabs/window_finder_impl.h"

gfx::NativeWindow GetLocalProcessWindowAtPointAsh(
    const gfx::Point& screen_point,
    const std::set<gfx::NativeWindow>& ignore) {
  gfx::NativeWindow window = ::ash::wm::GetRootWindowAt(screen_point);
  std::set<int> ignore_ash_ids = {ash::kShellWindowId_PhantomWindow,
                                  ash::kShellWindowId_OverlayContainer,
                                  ash::kShellWindowId_MouseCursorContainer};
  return GetLocalProcessWindowAtPointImpl(screen_point, ignore, ignore_ash_ids,
                                          window);
}
