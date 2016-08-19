// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/immersive_handler_factory_ash.h"

#include "ash/wm/immersive_focus_watcher.h"
#include "ash/wm/immersive_gesture_handler.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace ash {

ImmersiveHandlerFactoryAsh::ImmersiveHandlerFactoryAsh() {}

ImmersiveHandlerFactoryAsh::~ImmersiveHandlerFactoryAsh() {}

std::unique_ptr<ImmersiveFocusWatcher>
ImmersiveHandlerFactoryAsh::CreateFocusWatcher(
    ImmersiveFullscreenController* controller) {
  return base::MakeUnique<ImmersiveFocusWatcher>(controller);
}

std::unique_ptr<ImmersiveGestureHandler>
ImmersiveHandlerFactoryAsh::CreateGestureHandler(
    ImmersiveFullscreenController* controller) {
  return base::MakeUnique<ImmersiveGestureHandler>(controller);
}

}  // namespace ash
