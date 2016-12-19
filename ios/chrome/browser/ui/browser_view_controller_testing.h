// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_CONTROLLER_TESTING_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_CONTROLLER_TESTING_H_

#import "ios/chrome/browser/ui/browser_view_controller.h"

@interface BrowserViewController (TestingAdditions)

- (BOOL)testing_isLoading;

- (void)testing_focusOmnibox;

- (CGRect)testing_shareButtonAnchorRect;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_CONTROLLER_TESTING_H_
