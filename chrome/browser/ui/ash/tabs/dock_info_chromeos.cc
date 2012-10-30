// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/dock_info.h"

#include "chrome/browser/ui/ash/tabs/dock_info_ash.h"
#include "chrome/browser/ui/host_desktop.h"
#include "ui/aura/window.h"

// static
DockInfo DockInfo::GetDockInfoAtPoint(chrome::HostDesktopType host_desktop_type,
                                      const gfx::Point& screen_point,
                                      const std::set<gfx::NativeView>& ignore) {
  return chrome::ash::GetDockInfoAtPointAsh(screen_point, ignore);
}

// static
gfx::NativeView DockInfo::GetLocalProcessWindowAtPoint(
    chrome::HostDesktopType host_desktop_type,
    const gfx::Point& screen_point,
    const std::set<gfx::NativeView>& ignore) {
  return chrome::ash::GetLocalProcessWindowAtPointAsh(screen_point, ignore);
}

bool DockInfo::GetWindowBounds(gfx::Rect* bounds) const {
  if (!window())
    return false;
  *bounds = window_->bounds();
  return true;
}

void DockInfo::SizeOtherWindowTo(const gfx::Rect& bounds) const {
  window_->SetBounds(bounds);
}
