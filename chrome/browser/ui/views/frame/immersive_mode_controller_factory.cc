// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller_stub.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"

namespace chrome {

ImmersiveModeController* CreateImmersiveModeController(
    chrome::HostDesktopType host_desktop_type) {
  if (host_desktop_type == chrome::HOST_DESKTOP_TYPE_ASH)
    return new ImmersiveModeControllerAsh();

  return new ImmersiveModeControllerStub();
}

}  // namespace chrome
