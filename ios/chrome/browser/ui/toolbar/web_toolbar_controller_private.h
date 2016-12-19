// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_WEB_TOOLBAR_CONTROLLER_PRIVATE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_WEB_TOOLBAR_CONTROLLER_PRIVATE_H_

#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller.h"

// Private methods for unit tests.
@interface WebToolbarController (Testing)
- (BOOL)isForwardButtonEnabled;
- (BOOL)isBackButtonEnabled;
- (BOOL)isStarButtonSelected;
- (void)setUnitTesting:(BOOL)unitTesting;
- (CGFloat)loadProgressFraction;
- (std::string)getLocationText;
- (BOOL)isLoading;
- (BOOL)isPrerenderAnimationRunning;
- (OmniboxTextFieldIOS*)omnibox;
@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_WEB_TOOLBAR_CONTROLLER_PRIVATE_H_
