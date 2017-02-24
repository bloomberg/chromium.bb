// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_BUTTON_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_BUTTON_H_

#import <UIKit/UIKit.h>

#import "ios/clean/chrome/browser/ui/toolbar/toolbar_component_options.h"

// UIButton subclass used as a Toolbar component.
@interface ToolbarButton : UIButton

// Bitmask used for SizeClass visibility.
@property(nonatomic, assign) ToolbarComponentVisibility visibilityMask;
// Returns true if the ToolbarButton should be hidden in the current SizeClass.
@property(nonatomic, assign) BOOL hiddenInCurrentSizeClass;
// Returns true if the ToolbarButton should be hidden due to a current UI state
// or WebState.
@property(nonatomic, assign) BOOL hiddenInCurrentState;
// Returns a ToolbarButton using the three images parameters for their
// respective state.
+ (instancetype)toolbarButtonWithImageForNormalState:(UIImage*)normalImage
                            imageForHighlightedState:(UIImage*)highlightedImage
                               imageForDisabledState:(UIImage*)disabledImage;
// Checks if the ToolbarButton should be visible in the current SizeClass,
// afterwards it calls setHiddenForCurrentStateAndSizeClass if needed.
- (void)updateHiddenInCurrentSizeClass;
// Checks if the button should be visible based on its hiddenInCurrentSizeClass
// and hiddenInCurrentState properties, then updates its visibility accordingly.
- (void)setHiddenForCurrentStateAndSizeClass;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_BUTTON_H_
