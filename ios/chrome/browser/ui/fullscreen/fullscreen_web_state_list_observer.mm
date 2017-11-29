// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_state_list_observer.h"

#include "base/logging.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

FullscreenWebStateListObserver::FullscreenWebStateListObserver(
    FullscreenModel* model,
    WebStateList* web_state_list)
    : model_(model),
      web_state_list_(web_state_list),
      web_state_observer_(model) {
  DCHECK(model_);
  DCHECK(web_state_list_);
  web_state_list_->AddObserver(this);
}

FullscreenWebStateListObserver::~FullscreenWebStateListObserver() {
  // Disconnect() should be called before destruction.
  DCHECK(!web_state_list_);
}

void FullscreenWebStateListObserver::Disconnect() {
  web_state_list_->RemoveObserver(this);
  web_state_list_ = nullptr;
  web_state_observer_.SetWebState(nullptr);
}

void FullscreenWebStateListObserver::WebStateActivatedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int active_index,
    bool user_action) {
  web_state_observer_.SetWebState(new_web_state);
}
