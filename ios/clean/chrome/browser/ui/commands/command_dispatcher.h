// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_COMMAND_DISPATCHER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_COMMAND_DISPATCHER_H_

#import <Foundation/Foundation.h>

// CommandDispatcher allows coordinators to register as command handlers for
// specific selectors.  Other objects can call these methods on the dispatcher,
// which in turn will forward the call to the registered handler.
@interface CommandDispatcher : NSObject

// Registers the given |target| to receive forwarded messages for the given
// |selector|.
- (void)startDispatchingToTarget:(id)target forSelector:(SEL)selector;

// Removes all forwarding registrations for the given |target|.
- (void)stopDispatchingToTarget:(id)target;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_COMMAND_DISPATCHER_H_
