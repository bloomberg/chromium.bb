// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_NAVIGATION_COMMANDS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_NAVIGATION_COMMANDS_H_

// Commands relating to web page navigation.
@protocol NavigationCommands<NSObject>
@optional
// Goes back to the previous visited page.
- (void)goBack;
// Goes forward to the next page.
- (void)goForward;
// Reloads the current page.
- (void)reloadPage;
// Stops loading the page.
- (void)stopLoadingPage;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_NAVIGATION_COMMANDS_H_
