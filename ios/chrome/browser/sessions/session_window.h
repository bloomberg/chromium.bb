// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_WINDOW_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_WINDOW_H_

#import <Foundation/Foundation.h>

#include <memory>

#import "ios/web/web_state/web_state_impl.h"

// Encapsulates everything required to save a session "window". For iOS, there
// will only be one window at a time.
@interface SessionWindowIOS : NSObject<NSCoding>

// So that ownership transfer of WebStateImpls is explicit, SessionWindows
// are initialized "empty" (without any sessions) and sessions are added
// one at a time. For example:
//  SessionWindowIOS* window = [[SessionWindow alloc] init];
//  [window addSession:some_scoped_webstate_ptr.Pass()];
//  ...
//  [window setSelectedInex:mySelectedIndex];
- (void)addSession:(std::unique_ptr<web::WebStateImpl>)session;

- (void)clearSessions;

// Takes the first session stored in the reciever, removes it and passes
// ownership of it to the caller.
- (std::unique_ptr<web::WebStateImpl>)nextSession;

// The currently selected session. NSNotFound if the sessionWindow contains
// no sessions; otherwise 0 <= |selectedIndex| < |unclaimedSessions|.
@property(nonatomic) NSUInteger selectedIndex;
// A count of the remaining sessions that haven't been claimed.
@property(nonatomic, readonly) NSUInteger unclaimedSessions;

@end

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_WINDOW_H_
