// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_ACTIONS_NAVIGATION_ACTIONS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_ACTIONS_NAVIGATION_ACTIONS_H_

// Target/Action methods relating to web page navigation.
// (Actions should only be used to communicate into or between the View
// Controller layer).
@protocol NavigationActions
@optional
// Goes back to the previous visited page.
- (void)goBack:(id)sender;
// Goes forward to the next page.
- (void)goForward:(id)sender;
// Reloads the current page.
- (void)reload:(id)sender;
// Stops loading the page.
- (void)stop:(id)sender;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_ACTIONS_NAVIGATION_ACTIONS_H_
