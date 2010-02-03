// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/demos/framework/window.h"

namespace gpu {
namespace demos {

void Window::MainLoop() {
}

gfx::NativeWindow Window::CreateNativeWindow(const wchar_t* title,
                                             int width, int height) {
  return NULL;
}

gfx::PluginWindowHandle Window::PluginWindow(gfx::NativeWindow hwnd) {
  return gfx::kNullPluginWindow;
}

}  // namespace demos
}  // namespace gpu

