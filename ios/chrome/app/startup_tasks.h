// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_STARTUP_TASKS_H_
#define IOS_CHROME_APP_STARTUP_TASKS_H_

#import <Foundation/Foundation.h>

class ChromeBrowserState;

// Class handling all startup tasks.
@interface StartupTasks : NSObject

// Asynchronously finishes the browser state initialization by starting the
// deferred task runners.
+ (void)scheduleDeferredBrowserStateInitialization:
    (ChromeBrowserState*)browserState;
// Starts Omaha and, if first run, sets install time.  For official builds only.
- (void)initializeOmaha;
// Donate initial Intents.
- (void)donateIntents;
// Registers to receive UIApplicationWillResignActiveNotification.
- (void)registerForApplicationWillResignActiveNotification;

@end

#endif  // IOS_CHROME_APP_STARTUP_TASKS_H_
