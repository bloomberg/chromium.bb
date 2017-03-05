// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/wm_screen_util.h"

#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/root_window_controller.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace ash {
namespace wm {

gfx::Rect GetDisplayWorkAreaBoundsInParent(WmWindow* window) {
  display::Display display = window->GetDisplayNearestWindow();
  return window->GetParent()->ConvertRectFromScreen(display.work_area());
}

gfx::Rect GetDisplayBoundsInParent(WmWindow* window) {
  display::Display display = window->GetDisplayNearestWindow();
  return window->GetParent()->ConvertRectFromScreen(display.bounds());
}

gfx::Rect GetMaximizedWindowBoundsInParent(WmWindow* window) {
  if (window->GetRootWindowController()->HasShelf())
    return GetDisplayWorkAreaBoundsInParent(window);

  return GetDisplayBoundsInParent(window);
}

gfx::Rect GetDisplayBoundsWithShelf(WmWindow* window) {
  if (WmShell::Get()->IsInUnifiedMode()) {
    // In unified desktop mode, there is only one shelf in the first display.
    gfx::SizeF size(WmShell::Get()->GetFirstDisplay().size());
    float scale = window->GetRootWindow()->GetBounds().height() / size.height();
    size.Scale(scale, scale);
    return gfx::Rect(gfx::ToCeiledSize(size));
  }

  return window->GetRootWindow()->GetBounds();
}

}  // namespace wm
}  // namespace ash
