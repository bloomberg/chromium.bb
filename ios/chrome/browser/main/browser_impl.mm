// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/browser_impl.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/tabs/tab_model.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BrowserImpl::BrowserImpl(ios::ChromeBrowserState* browser_state,
                         TabModel* tab_model)
    : browser_state_(browser_state), tab_model_(tab_model) {
  DCHECK(browser_state_);
  DCHECK(tab_model_);
  web_state_list_ = tab_model_.webStateList;
}

BrowserImpl::~BrowserImpl() = default;

ios::ChromeBrowserState* BrowserImpl::GetBrowserState() const {
  return browser_state_;
}

TabModel* BrowserImpl::GetTabModel() const {
  return tab_model_;
}

WebStateList* BrowserImpl::GetWebStateList() const {
  return web_state_list_;
}

// static
std::unique_ptr<Browser> Browser::Create(ios::ChromeBrowserState* browser_state,
                                         TabModel* tab_model) {
  return std::make_unique<BrowserImpl>(browser_state, tab_model);
}
