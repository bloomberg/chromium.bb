// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/platform_util.h"

#include "base/logging.h"
#include "ui/aura/window.h"

namespace platform_util {

void ShowItemInFolder(const FilePath& full_path) {
  // TODO(beng):
  NOTIMPLEMENTED();
}

void OpenItem(const FilePath& full_path) {
  // TODO(beng):
  NOTIMPLEMENTED();
}

void OpenExternal(const GURL& url) {
  // TODO(beng):
  NOTIMPLEMENTED();
}

gfx::NativeWindow GetTopLevel(gfx::NativeView view) {
  // TODO(beng):
  NOTIMPLEMENTED();
  return NULL;
}

gfx::NativeView GetParent(gfx::NativeView view) {
  return view->parent();
}

bool IsWindowActive(gfx::NativeWindow window) {
  // TODO(beng):
  NOTIMPLEMENTED();
  return false;
}

void ActivateWindow(gfx::NativeWindow window) {
  // TODO(beng):
  NOTIMPLEMENTED();
}

bool IsVisible(gfx::NativeView view) {
  return view->visibility() != aura::Window::VISIBILITY_HIDDEN;
}

}  // namespace platform_util
