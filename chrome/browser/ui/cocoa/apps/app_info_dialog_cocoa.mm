// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/ui/apps/app_info_dialog.h"

void ShowAppInfoInAppList(gfx::NativeWindow parent,
                          const gfx::Rect& app_list_bounds,
                          Profile* profile,
                          const extensions::Extension* app,
                          const base::Closure& close_callback) {
  // TODO(sashab): Implement the App Info dialog on Mac.
  NOTIMPLEMENTED();
}

void ShowAppInfoInNativeDialog(gfx::NativeWindow parent,
                               const gfx::Size& size,
                               Profile* profile,
                               const extensions::Extension* app,
                               const base::Closure& close_callback) {
  // TODO(sashab): Implement the App Info dialog on Mac.
  NOTIMPLEMENTED();
}
