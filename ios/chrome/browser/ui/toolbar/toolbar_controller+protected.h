// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONTROLLER_PROTECTED_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONTROLLER_PROTECTED_H_

#import "ios/chrome/browser/ui/toolbar/toolbar_controller.h"

@interface ToolbarController (Protected)

// Animation key used for toolbar transition animations.
extern NSString* const kToolbarTransitionAnimationKey;

// An array of CALayers that are currently animating under
// kToolbarTransitionAnimationKey.
@property(nonatomic, readonly) NSMutableArray* transitionLayers;

// Update share button visibility and |standardButtons_| array.
- (void)updateStandardButtons;

// Height and Y offset to account for the status bar. Overridden by subclasses
// if the toolbar shouldn't extend through the status bar.
- (CGFloat)statusBarOffset;

// Called when buttons are pressed. Records action metrics.
// Subclasses must call |super| if they override this method.
- (IBAction)recordUserMetrics:(id)sender;

// Returns the stackButton (a.k.a. tabSwitcherButton). Used by subclasses to
// set display or dismiss target-actions.
- (UIButton*)stackButton;

// Called whenever one of the standard controls is triggered. Does nothing,
// but can be overridden by subclasses to clear any state (e.g., close menus).
- (void)standardButtonPressed:(UIButton*)sender;

// Returns the area that subclasses should use for adding their own
// controls. Nothing should be added outside this frame without first
// calling |setStandardControlsVisible:NO|.
- (CGRect)specificControlsArea;

// Sets the standard control set to be visible or invisible. Uses alpha, so it
// can be animated if called from an animation context. Intended for use by
// subclasses that need to temporarily take over the entire toolbar.
- (void)setStandardControlsVisible:(BOOL)visible;

// Sets the standard control to a particular alpha value. Intended for use by
// subclasses that need to temporarily take over the entire toolbar.
- (void)setStandardControlsAlpha:(CGFloat)alpha;

// Returns the UIImage from the resources bundle for the |imageEnum| and
// |state|.  Uses the toolbar's current style.
- (UIImage*)imageForImageEnum:(int)imageEnum
                     forState:(ToolbarButtonUIState)state;

// Returns the image enum for a given button object.  If the user taps a
// button before its secondary images have been loaded, the image(s) will be
// loaded then, synchronously.  Called by -standardButtonPressed.
- (int)imageEnumForButton:(UIButton*)button;

// Returns the resource ID for the image with enum |index|, corresponding to
// |style| and |state|.  Returns 0 if there is not a corresponding ID.
// If a subclass extends the enum ToolbarButtonName, it must also override
// this method to provide the correct mapping from enum to resource ID.
- (int)imageIdForImageEnum:(int)index
                     style:(ToolbarControllerStyle)style
                  forState:(ToolbarButtonUIState)state;

// Creates a hash of the UI state of the toolbar.
- (uint32_t)snapshotHash;

// Adds transition animations to every UIButton in |containerView| with a
// nonzero opacity.
- (void)animateTransitionForButtonsInView:(UIView*)containerView
                     containerBeginBounds:(CGRect)containerBeginBounds
                       containerEndBounds:(CGRect)containerEndBounds
                          transitionStyle:(ToolbarTransitionStyle)style;

// Add position and opacity animations to |view|'s layer. The opacity
// animation goes from 0 to 1. The position animation goes from
// [view.layer.position offset in the leading direction by |leadingOffset|)
// to view.layer.position. Both animations occur after |delay| seconds.
- (void)fadeInView:(UIView*)view
    fromLeadingOffset:(LayoutOffset)leadingOffset
         withDuration:(NSTimeInterval)duration
           afterDelay:(NSTimeInterval)delay;

// Animates the tools menu button and stack button for full bleed omnibox
// animation used for Material.
- (void)animateStandardControlsForOmniboxExpansion:(BOOL)growOmnibox;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONTROLLER_PROTECTED_H_
