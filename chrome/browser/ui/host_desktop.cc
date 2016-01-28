// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/host_desktop.h"

#include "build/build_config.h"

namespace chrome {

HostDesktopType GetHostDesktopTypeForNativeView(gfx::NativeView native_view) {
  // TODO(scottmg): This file is being removed: http://crbug.com/558054.
  return HOST_DESKTOP_TYPE_NATIVE;
}

HostDesktopType GetHostDesktopTypeForNativeWindow(
    gfx::NativeWindow native_window) {
  // TODO(scottmg): This file is being removed: http://crbug.com/558054.
  return HOST_DESKTOP_TYPE_NATIVE;
}

HostDesktopType GetActiveDesktop() {
  // TODO(scottmg): This file is being removed: http://crbug.com/558054.
  return HOST_DESKTOP_TYPE_NATIVE;
}

}  // namespace chrome
