// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NO_TABS_NO_TABS_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NO_TABS_NO_TABS_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class NoTabsToolbarController;

@interface NoTabsController : NSObject

// Desginated initializer. Installs the No-Tabs UI in the given |view|.
- (id)initWithView:(UIView*)view;

// Sets whether or not the No-Tabs UI displays a mode toggle switch.  Passes
// through to the underlying NoTabsToolbarController.
- (void)setHasModeToggleSwitch:(BOOL)hasModeToggle;

// Sets up and installs an animation that will animate between the given
// buttons.  If |show| is YES, the animation is set up as a show animation;
// otherwise, it is set up as a dismiss animation.
- (void)installAnimationImageForButton:(UIButton*)fromButton
                                inView:(UIView*)view
                                  show:(BOOL)show;

// TODO(blundell): This method should be internalized if the NoTabsController
// becomes part of the responder chain and can catch the command to show the
// tools menu directly via |chromeExecuteCommand|.
// Shows the tools menu popup.
- (void)showToolsMenuPopup;

// Dismisses the tools popup if it is open.
- (void)dismissToolsMenuPopup;

// Must be called before starting the show animation in order to move views into
// their pre-animation positions.
- (void)prepareForShowAnimation;

// Shows the No-Tabs UI.  Can be called from within an animation block to
// animate the show.
- (void)showNoTabsUI;

// Must be called after the show animation finishes to allow this controller to
// clean up any animation-related state.
- (void)showAnimationDidFinish;

// Must be called before starting the dismiss animation in order to move views
// into their pre-animation positions.
- (void)prepareForDismissAnimation;

// Dismisses the No-Tabs UI.  Can be called from within an animation block to
// animate the dismiss.
- (void)dismissNoTabsUI;

// Must be called after the dismiss animation finishes to allow this controller
// to clean up any animation-related state.
- (void)dismissAnimationDidFinish;

@end

#endif  // IOS_CHROME_BROWSER_UI_NO_TABS_NO_TABS_CONTROLLER_H_
