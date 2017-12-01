// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"

#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/ui/broadcaster/chrome_broadcast_observer_bridge.h"
#import "ios/chrome/browser/ui/broadcaster/chrome_broadcaster.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_state_list_observer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The key under which FullscreenControllers are associated with their user
// data.
const void* const kFullscreenControllerKey = &kFullscreenControllerKey;
}  // namespace

// static
void FullscreenController::CreateForUserData(
    base::SupportsUserData* user_data) {
  DCHECK(user_data);
  if (!FullscreenController::FromUserData(user_data)) {
    user_data->SetUserData(kFullscreenControllerKey,
                           base::WrapUnique(new FullscreenController()));
  }
}

// static
FullscreenController* FullscreenController::FromUserData(
    base::SupportsUserData* user_data) {
  return static_cast<FullscreenController*>(
      user_data->GetUserData(kFullscreenControllerKey));
}

FullscreenController::FullscreenController()
    : broadcaster_([[ChromeBroadcaster alloc] init]),
      model_(base::MakeUnique<FullscreenModel>()),
      bridge_([[ChromeBroadcastOberverBridge alloc]
          initWithObserver:model_.get()]) {
  DCHECK(broadcaster_);
  [broadcaster_ addObserver:bridge_
                forSelector:@selector(broadcastContentScrollOffset:)];
  [broadcaster_ addObserver:bridge_
                forSelector:@selector(broadcastScrollViewIsScrolling:)];
  [broadcaster_ addObserver:bridge_
                forSelector:@selector(broadcastScrollViewIsDragging:)];
  [broadcaster_ addObserver:bridge_
                forSelector:@selector(broadcastToolbarHeight:)];
}

FullscreenController::~FullscreenController() {
  [broadcaster_ removeObserver:bridge_
                   forSelector:@selector(broadcastContentScrollOffset:)];
  [broadcaster_ removeObserver:bridge_
                   forSelector:@selector(broadcastScrollViewIsScrolling:)];
  [broadcaster_ removeObserver:bridge_
                   forSelector:@selector(broadcastScrollViewIsDragging:)];
  [broadcaster_ removeObserver:bridge_
                   forSelector:@selector(broadcastToolbarHeight:)];
}

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
  return model_->enabled();
}

void FullscreenController::IncrementDisabledCounter() {
  model_->IncrementDisabledCounter();
}

void FullscreenController::DecrementDisabledCounter() {
  model_->DecrementDisabledCounter();
}
