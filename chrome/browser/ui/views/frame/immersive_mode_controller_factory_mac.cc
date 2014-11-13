// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_stub.h"

namespace chrome {

ImmersiveModeController* CreateImmersiveModeController(
    chrome::HostDesktopType host_desktop_type) {
  return new ImmersiveModeControllerStub();
}

}  // namespace chrome
