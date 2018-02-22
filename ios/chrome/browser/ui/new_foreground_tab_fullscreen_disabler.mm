// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/new_foreground_tab_fullscreen_disabler.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_controller.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NewForegroundTabFullscreenDisabler::NewForegroundTabFullscreenDisabler(
    WebStateList* web_state_list,
    FullscreenController* controller)
    : web_state_list_(web_state_list), controller_(controller) {
  DCHECK(web_state_list_);
  DCHECK(controller_);
  web_state_list_->AddObserver(this);
}

NewForegroundTabFullscreenDisabler::~NewForegroundTabFullscreenDisabler() {
  // Disconnect() is expected to be called before destruction.
  DCHECK(!web_state_list_);
  DCHECK(!controller_);
  DCHECK(!disabler_);
}

void NewForegroundTabFullscreenDisabler::Disconnect() {
  web_state_list_->RemoveObserver(this);
  web_state_list_ = nullptr;
  controller_ = nullptr;
  disabler_ = nullptr;
}

void NewForegroundTabFullscreenDisabler::FullscreenDisablingAnimationDidFinish(
    AnimatedScopedFullscreenDisabler* disabler) {
  DCHECK_EQ(disabler_.get(), disabler);
  disabler_ = nullptr;
}

void NewForegroundTabFullscreenDisabler::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool activating) {
  DCHECK_EQ(web_state_list_, web_state_list);
  if (activating && controller_->IsEnabled()) {
    disabler_ = std::make_unique<AnimatedScopedFullscreenDisabler>(controller_);
    disabler_->AddObserver(this);
    disabler_->StartAnimation();
  }
}
