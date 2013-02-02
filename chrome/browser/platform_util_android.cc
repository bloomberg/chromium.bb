// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/platform_util.h"
#include "content/public/browser/android/content_view_core.h"

namespace platform_util {

// TODO: crbug/115682 to track implementation of the following methods.

void ShowItemInFolder(const base::FilePath& full_path) {
  NOTIMPLEMENTED();
}

void OpenItem(const base::FilePath& full_path) {
  NOTIMPLEMENTED();
}

void OpenExternal(const GURL& url) {
  NOTIMPLEMENTED();
}

gfx::NativeWindow GetTopLevel(gfx::NativeView view) {
  NOTIMPLEMENTED();
  return view->GetWindowAndroid();
}

gfx::NativeView GetParent(gfx::NativeView view) {
  NOTIMPLEMENTED();
  return view;
}

bool IsWindowActive(gfx::NativeWindow window) {
  NOTIMPLEMENTED();
  return false;
}

void ActivateWindow(gfx::NativeWindow window) {
  NOTIMPLEMENTED();
}

bool IsVisible(gfx::NativeView view) {
  NOTIMPLEMENTED();
  return true;
}

} // namespace platform_util

