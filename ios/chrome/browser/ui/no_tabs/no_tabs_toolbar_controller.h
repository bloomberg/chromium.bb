// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NO_TABS_NO_TABS_TOOLBAR_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NO_TABS_NO_TABS_TOOLBAR_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/toolbar/toolbar_controller.h"

// Toolbar controller for the no tabs view, adding a new tab button and a mode
// toggle button..
@interface NoTabsToolbarController : ToolbarController

// Designated initializer.
- (instancetype)initWithNoTabs;

// Applies the given |transform| to all of the controls in this toolbar.
- (void)setControlsTransform:(CGAffineTransform)transform;

// Sets whether or not this toolbar displays a mode toggle switch.
- (void)setHasModeToggleSwitch:(BOOL)hasModeToggle;

// Return the button for toggling between normal and incognito.
- (UIButton*)modeToggleButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_NO_TABS_NO_TABS_TOOLBAR_CONTROLLER_H_
