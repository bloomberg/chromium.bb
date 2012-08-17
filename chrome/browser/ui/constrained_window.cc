// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/constrained_window.h"

#include "base/logging.h"

SkColor ConstrainedWindow::GetBackgroundColor() {
  return SkColorSetRGB(0xfb, 0xfb, 0xfb);
}

SkColor ConstrainedWindow::GetTextColor() {
  return SkColorSetRGB(0x33, 0x33, 0x33);
}

void ConstrainedWindow::FocusConstrainedWindow() {
}

gfx::NativeWindow ConstrainedWindow::GetNativeWindow() {
  NOTREACHED();
  return NULL;
}
