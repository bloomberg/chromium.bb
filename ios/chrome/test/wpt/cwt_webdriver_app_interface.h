// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_WPT_CWT_WEBDRIVER_APP_INTERFACE_H_
#define IOS_CHROME_TEST_WPT_CWT_WEBDRIVER_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// Methods used by CWTRequestHandler to perform browser-level actions such as
// opening and closing tabs, navigating to a URL, and injecting JavaScript.
// These methods run on a background thread in the app process in order to
// avoid deadlock while waiting for actions to complete on the main thread.
@interface CWTWebDriverAppInterface : NSObject

// Loads the given URL in the current tab. Returns true if the load completes
// within |timeout| seconds, and false otherwise.
+ (NSError*)loadURL:(NSString*)URL timeoutInSeconds:(NSTimeInterval)timeout;

@end

#endif  // IOS_CHROME_TEST_WPT_CWT_WEBDRIVER_APP_INTERFACE_H_
