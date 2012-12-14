// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/constrained_window.h"

#include "base/logging.h"

void ConstrainedWindow::FocusConstrainedWindow() {
}

void ConstrainedWindow::PulseConstrainedWindow() {
}

bool ConstrainedWindow::CanShowConstrainedWindow() {
  return true;
}

gfx::NativeWindow ConstrainedWindow::GetNativeWindow() {
  NOTREACHED();
  return NULL;
}
