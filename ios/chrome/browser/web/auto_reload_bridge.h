// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_WEB_AUTO_RELOAD_BRIDGE_H_
#define IOS_CHROME_BROWSER_WEB_AUTO_RELOAD_BRIDGE_H_

#import <Foundation/Foundation.h>

#include "ios/chrome/browser/web/auto_reload_controller.h"
#include "url/gurl.h"

@class Tab;

// AutoReloadBridge is the interface between AutoReloadController and the
// outside world and isolates AutoReloadController from its dependencies.
// An AutoReloadBridge is responsible for network state tracking, for receiving
// and passing on events from its owning Tab, and for passing reload requests
// back to its owning Tab.
@interface AutoReloadBridge : NSObject<AutoReloadDelegate>

// Initialize an instance of this class owned by the supplied Tab, which must
// not be nil.
- (instancetype)initWithTab:(Tab*)tab;

// Called by the owning Tab whenever a load starts.
- (void)loadStartedForURL:(const GURL&)url;

// Called by the owning Tab whenever a load finishes.
- (void)loadFinishedForURL:(const GURL&)url wasPost:(BOOL)wasPost;

// Called by the owning Tab whenever a load fails.
- (void)loadFailedForURL:(const GURL&)url wasPost:(BOOL)wasPost;

@end

#endif  // IOS_CHROME_BROWSER_WEB_AUTO_RELOAD_BRIDGE_H_
