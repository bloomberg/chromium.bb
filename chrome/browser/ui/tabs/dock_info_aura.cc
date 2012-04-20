// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/dock_info.h"

#include "ui/aura/window.h"

#if !defined(USE_ASH)

// static
DockInfo DockInfo::GetDockInfoAtPoint(const gfx::Point& screen_point,
                                      const std::set<gfx::NativeView>& ignore) {
  // TODO(beng):
  NOTIMPLEMENTED();
  return DockInfo();
}

// static
gfx::NativeView DockInfo::GetLocalProcessWindowAtPoint(
    const gfx::Point& screen_point,
    const std::set<gfx::NativeView>& ignore) {
  NOTIMPLEMENTED();
  return NULL;
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

#endif
