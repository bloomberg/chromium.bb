// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/window_finder.h"

#include "chrome/browser/ui/host_desktop.h"
#include "ui/aura/window.h"

aura::Window* GetLocalProcessWindowAtPointAsh(
    const gfx::Point& screen_point,
    const std::set<aura::Window*>& ignore);

aura::Window* GetLocalProcessWindowAtPoint(
    chrome::HostDesktopType host_desktop_type,
    const gfx::Point& screen_point,
    const std::set<aura::Window*>& ignore) {
  return GetLocalProcessWindowAtPointAsh(screen_point, ignore);
}
