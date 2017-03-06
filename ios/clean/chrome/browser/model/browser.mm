// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/model/browser.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

Browser::Browser(ios::ChromeBrowserState* browser_state)
    : web_state_list_(WebStateList::WebStateOwned),
      browser_state_(browser_state) {
  DCHECK(browser_state_);
}

Browser::~Browser() = default;
