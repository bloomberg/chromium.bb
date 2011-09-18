// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/dock_info.h"

#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "ui/gfx/screen.h"
#if defined(OS_WIN)
#include "base/win/scoped_gdi_object.h"
#endif

// DockInfo -------------------------------------------------------------------

// static
DockInfo DockInfo::GetDockInfoAtPoint(const gfx::Point& screen_point,
                                      const std::set<gfx::NativeView>& ignore) {
  // TODO(beng):
  NOTIMPLEMENTED();
  return DockInfo();
}

gfx::NativeView DockInfo::GetLocalProcessWindowAtPoint(
    const gfx::Point& screen_point,
    const std::set<gfx::NativeView>& ignore) {
  // TODO(beng):
  NOTIMPLEMENTED();
  return NULL;
}

bool DockInfo::GetWindowBounds(gfx::Rect* bounds) const {
  // TODO(beng):
  NOTIMPLEMENTED();
  return true;
}

void DockInfo::SizeOtherWindowTo(const gfx::Rect& bounds) const {
  // TODO(beng):
  NOTIMPLEMENTED();
}
