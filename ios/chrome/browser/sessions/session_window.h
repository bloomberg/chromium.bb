// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_WINDOW_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_WINDOW_H_

#import <Foundation/Foundation.h>

#include <memory>

#import "ios/web/web_state/web_state_impl.h"

@class CRWSessionStorage;

// Encapsulates everything required to save a session "window". For iOS, there
// will only be one window at a time.
@interface SessionWindowIOS : NSObject<NSCoding>

// Adds a serialized session to be written to disk.  SessionWindows are
// initialized "empty" (without any sessions) and sessions are added one at a
// time.  For example:
//  SessionWindowIOS* window = [[SessionWindow alloc] init];
//  [window addSerializedSessionStorage:session_storage];
//  ...
//  [window setSelectedIndex:mySelectedIndex];
- (void)addSerializedSessionStorage:(CRWSessionStorage*)session;

// Clears all added sessions.
- (void)clearSessions;

// The serialized session objects.
@property(nonatomic, readonly) NSArray* sessions;

// The currently selected session. NSNotFound if the sessionWindow contains
// no sessions; otherwise 0 <= |selectedIndex| < |unclaimedSessions|.
@property(nonatomic) NSUInteger selectedIndex;

@end

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_WINDOW_H_
