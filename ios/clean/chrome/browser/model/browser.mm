// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/model/browser.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#import "ios/clean/chrome/browser/model/browser_web_state_list_delegate.h"
#import "ios/shared/chrome/browser/tabs/web_state_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

Browser::Browser(ios::ChromeBrowserState* browser_state)
    : browser_state_(browser_state) {
  DCHECK(browser_state_);
  web_state_list_delegate_ =
      base::MakeUnique<BrowserWebStateListDelegate>(this);
  web_state_list_ = base::MakeUnique<WebStateList>(
      web_state_list_delegate_.get(), WebStateList::WebStateOwned);
}

Browser::~Browser() = default;
