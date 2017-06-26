// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_handler_factory_mus.h"

#include "ash/public/cpp/immersive/immersive_focus_watcher.h"
#include "ash/public/cpp/immersive/immersive_gesture_handler.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"

ImmersiveHandlerFactoryMus::ImmersiveHandlerFactoryMus() {}

ImmersiveHandlerFactoryMus::~ImmersiveHandlerFactoryMus() {}

std::unique_ptr<ash::ImmersiveFocusWatcher>
ImmersiveHandlerFactoryMus::CreateFocusWatcher(
    ash::ImmersiveFullscreenController* controller) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<ash::ImmersiveGestureHandler>
ImmersiveHandlerFactoryMus::CreateGestureHandler(
    ash::ImmersiveFullscreenController* controller) {
  NOTIMPLEMENTED();
  return nullptr;
}
