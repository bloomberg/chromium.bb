// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_stub.h"

#if defined(USE_ASH)
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"
#endif

namespace chrome {

ImmersiveModeController* CreateImmersiveModeController() {
#if defined(USE_ASH)
  if (!IsRunningInMash())
    return new ImmersiveModeControllerAsh();

  // TODO: http://crbug.com/548435
  NOTIMPLEMENTED();
#endif  // USE_ASH
  return new ImmersiveModeControllerStub();
}

}  // namespace chrome
