// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"

#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/ui/broadcaster/chrome_broadcast_observer_bridge.h"
#import "ios/chrome/browser/ui/broadcaster/chrome_broadcaster.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_mediator.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_state_list_observer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FullscreenController::FullscreenController()
    : broadcaster_([[ChromeBroadcaster alloc] init]),
      model_(base::MakeUnique<FullscreenModel>()),
      bridge_(
          [[ChromeBroadcastOberverBridge alloc] initWithObserver:model_.get()]),
      mediator_(base::MakeUnique<FullscreenMediator>(this, model_.get())) {
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

FullscreenController::~FullscreenController() = default;

void FullscreenController::SetWebStateList(WebStateList* web_state_list) {
  if (web_state_list_observer_)
    web_state_list_observer_->Disconnect();
  web_state_list_ = web_state_list;
  web_state_list_observer_ =
      web_state_list_ ? base::MakeUnique<FullscreenWebStateListObserver>(
                            model_.get(), web_state_list_)
                      : nullptr;
}

void FullscreenController::AddObserver(FullscreenControllerObserver* observer) {
  mediator_->AddObserver(observer);
}

void FullscreenController::RemoveObserver(
    FullscreenControllerObserver* observer) {
  mediator_->RemoveObserver(observer);
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

void FullscreenController::Shutdown() {
  mediator_->Disconnect();
  if (web_state_list_observer_)
    web_state_list_observer_->Disconnect();
  [broadcaster_ removeObserver:bridge_
                   forSelector:@selector(broadcastContentScrollOffset:)];
  [broadcaster_ removeObserver:bridge_
                   forSelector:@selector(broadcastScrollViewIsScrolling:)];
  [broadcaster_ removeObserver:bridge_
                   forSelector:@selector(broadcastScrollViewIsDragging:)];
  [broadcaster_ removeObserver:bridge_
                   forSelector:@selector(broadcastToolbarHeight:)];
}
