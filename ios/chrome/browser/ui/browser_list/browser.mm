// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_list/browser.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"

#import "ios/chrome/browser/ui/browser_list/browser_web_state_list_delegate.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/shared/chrome/browser/ui/broadcaster/chrome_broadcaster.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

Browser::Browser(ios::ChromeBrowserState* browser_state)
    : broadcaster_([[ChromeBroadcaster alloc] init]),
      dispatcher_([[CommandDispatcher alloc] init]),
      browser_state_(browser_state) {
  DCHECK(browser_state_);
  web_state_list_delegate_ =
      base::MakeUnique<BrowserWebStateListDelegate>(this);
  web_state_list_ =
      base::MakeUnique<WebStateList>(web_state_list_delegate_.get());
}

Browser::~Browser() = default;
