// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NO_TABS_NO_TABS_CONTROLLER_TESTING_H_
#define IOS_CHROME_BROWSER_UI_NO_TABS_NO_TABS_CONTROLLER_TESTING_H_

// Exposes necessary methods used for testing.
@interface NoTabsController (TestingAdditions)
- (UIButton*)modeToggleButton;
- (UIView*)toolbarView;
- (UIView*)backgroundView;
@end

#endif  // IOS_CHROME_BROWSER_UI_NO_TABS_NO_TABS_CONTROLLER_TESTING_H_
