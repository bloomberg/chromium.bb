// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONTROLLER_PRIVATE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONTROLLER_PRIVATE_H_

#import <UIKit/UIKit.h>

// Private interface used by tests.
@interface ToolbarController (TestingSupport)
// Returns the button opening the stackview. Used for tests.
- (UIButton*)stackButton;
@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONTROLLER_PRIVATE_H_
