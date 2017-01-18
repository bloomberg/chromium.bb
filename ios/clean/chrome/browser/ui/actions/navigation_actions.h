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
- (void)goBack:(id)sender;
- (void)goForward:(id)sender;
- (void)reload:(id)sender;
@end
//

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_ACTIONS_NAVIGATION_ACTIONS_H_
