// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include "base/logging.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/window_util.h"

namespace platform_util {

gfx::NativeWindow GetTopLevel(gfx::NativeView view) {
  return view->GetToplevelWindow();
}

gfx::NativeView GetParent(gfx::NativeView view) {
  return view->parent();
}

bool IsWindowActive(gfx::NativeWindow window) {
  return aura_shell::IsActiveWindow(window);
}

void ActivateWindow(gfx::NativeWindow window) {
  aura_shell::ActivateWindow(window);
}

bool IsVisible(gfx::NativeView view) {
  return view->IsVisible();
}

}  // namespace platform_util
