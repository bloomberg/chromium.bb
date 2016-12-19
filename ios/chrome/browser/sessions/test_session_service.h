// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_TEST_SESSION_SERVICE_H_
#define IOS_CHROME_BROWSER_SESSIONS_TEST_SESSION_SERVICE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/sessions/session_service.h"

// Testing subclass of SessionService that immediately consumes and archives
// to nowhere session windows passed into its
// -saveWindow:forBrowserState:immediately: method.
@interface TestSessionService : SessionServiceIOS
@end

#endif  // IOS_CHROME_BROWSER_SESSIONS_TEST_SESSION_SERVICE_H_
