// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller_stub.h"

namespace chrome {

ImmersiveModeController* CreateImmersiveModeController(
    chrome::HostDesktopType host_desktop_type) {
  // TODO(bshe): Implement for Android. See crbug.com/559179.
  return new ImmersiveModeControllerStub();
}

}  // namespace chrome
