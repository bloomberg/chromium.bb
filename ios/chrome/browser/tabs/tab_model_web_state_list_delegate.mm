// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_model_web_state_list_delegate.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TabModelWebStateListDelegate::TabModelWebStateListDelegate(TabModel* tab_model)
    : tab_model_(tab_model) {
  DCHECK(tab_model_);
}

TabModelWebStateListDelegate::~TabModelWebStateListDelegate() = default;

void TabModelWebStateListDelegate::WillAddWebState(web::WebState* web_state) {}

void TabModelWebStateListDelegate::WebStateDetached(web::WebState* web_state) {}
