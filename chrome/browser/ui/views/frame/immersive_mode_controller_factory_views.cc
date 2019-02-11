// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_stub.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"
#endif

namespace chrome {

std::unique_ptr<ImmersiveModeController> CreateImmersiveModeController() {
#if defined(OS_CHROMEOS)
  return std::make_unique<ImmersiveModeControllerAsh>();
#else
  return std::make_unique<ImmersiveModeControllerStub>();
#endif  // OS_CHROMEOS
}

}  // namespace chrome
