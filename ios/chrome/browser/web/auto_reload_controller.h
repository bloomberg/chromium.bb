// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_AUTO_RELOAD_CONTROLLER_H_
#define IOS_CHROME_BROWSER_WEB_AUTO_RELOAD_CONTROLLER_H_

#import <Foundation/Foundation.h>

#include "url/gurl.h"

// AutoReloadDelegate is the interface by which AutoReloadController affects the
// outside world. Tests can use a mock object implementing this protocol;
// normally, this is provided by AutoReloadBridge.
@protocol AutoReloadDelegate

// When called, this method should reload the tab being tracked (see the class
// comment on AutoReloadController below).
- (void)reload;

@end

// AutoReloadController implements the auto-reload logic. An
// AutoReloadController instance receives state change notifications from an
// AutoReloadBridge and calls back to its AutoReloadDelegate (in practice the
// same object as the AutoReloadBridge) to issue reload requests. Conceptually,
// an AutoReloadController tracks the state of a single tab.
@interface AutoReloadController : NSObject

// Initialize this object with a supplied delegate and initial online status.
- (instancetype)initWithDelegate:(id<AutoReloadDelegate>)delegate
                    onlineStatus:(BOOL)onlineStatus;

// This method notifies this object that a page load started for |url|.
- (void)loadStartedForURL:(const GURL&)url;

// This method notifies this object that a page load finished for |url|.
- (void)loadFinishedForURL:(const GURL&)url wasPost:(BOOL)wasPost;

// This method notifies this object that a page load failed for |url|.
- (void)loadFailedForURL:(const GURL&)url wasPost:(BOOL)wasPost;

// This method notifies this object that the device's network state changed to
// |online|.
- (void)networkStateChangedToOnline:(BOOL)online;

@end

#endif  // IOS_CHROME_BROWSER_WEB_AUTO_RELOAD_CONTROLLER_H_
