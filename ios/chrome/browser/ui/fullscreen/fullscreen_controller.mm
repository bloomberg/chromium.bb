// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FullscreenController::FullscreenController() {}

FullscreenController::~FullscreenController() {}

void FullscreenController::AddObserver(FullscreenControllerObserver* observer) {
  // TODO(crbug.com/785671): Use FullscreenControllerObserverManager to keep
  // track of observers.
}

void FullscreenController::RemoveObserver(
    FullscreenControllerObserver* observer) {
  // TODO(crbug.com/785671): Use FullscreenControllerObserverManager to keep
  // track of observers.
}

bool FullscreenController::IsEnabled() const {
  // TODO(crbug.com/785665): Use FullscreenModel to track enabled state.
  return false;
}

void FullscreenController::IncrementDisabledCounter() {
  // TODO(crbug.com/785665): Use FullscreenModel to track enabled state.
}

void FullscreenController::DecrementDisabledCounter() {
  // TODO(crbug.com/785665): Use FullscreenModel to track enabled state.
}
