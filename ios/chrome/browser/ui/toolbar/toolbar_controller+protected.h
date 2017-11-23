// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONTROLLER_PROTECTED_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONTROLLER_PROTECTED_H_

#import "ios/chrome/browser/ui/toolbar/public/toolbar_controller_constants.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_controller.h"

@interface ToolbarController (Protected)

// An array of CALayers that are currently animating under
// kToolbarTransitionAnimationKey.
@property(nonatomic, readonly) NSMutableArray* transitionLayers;

// UIViewPropertyAnimator for expanding the location bar.
@property(nonatomic, strong)
    UIViewPropertyAnimator* omniboxExpanderAnimator API_AVAILABLE(ios(10.0));

// UIViewPropertyAnimator for contracting the location bar.
@property(nonatomic, strong)
    UIViewPropertyAnimator* omniboxContractorAnimator API_AVAILABLE(ios(10.0));

// The view containing all the content of the toolbar. It respects the trailing
// and leading anchors of the safe area.
@property(nonatomic, readonly, strong) UIView* contentView;

// Update share button visibility and |standardButtons_| array.
- (void)updateStandardButtons;

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

// Animates in the standard Toolbar buttons when the Location bar is
// contracting.
- (void)configureFadeInAnimation API_AVAILABLE(ios(10.0));

// Animates out the standard Toolbar buttons when the Location bar is expanding.
- (void)configureFadeOutAnimation API_AVAILABLE(ios(10.0));

// Sets up |button| with images named by the given |imageEnum| and the current
// toolbar style.  Sets images synchronously for |initialState|, and
// asynchronously for the other states. Optionally sets the image for the
// disabled state as well.  Meant to be called during initialization.
// Note:  |withImageEnum| should be one of the ToolbarButtonName values, or an
// extended value provided by a subclass.  It is an int to support
// "subclassing" of the enum and overriding helper functions.
- (void)setUpButton:(UIButton*)button
       withImageEnum:(int)imageEnum
     forInitialState:(UIControlState)initialState
    hasDisabledImage:(BOOL)hasDisabledImage
       synchronously:(BOOL)synchronously;

// TRUE if |imageEnum| should be flipped when in RTL layout.
// Currently none of this class' images have this property, but subclasses
// can override this method if they need to flip some of their images.
- (BOOL)imageShouldFlipForRightToLeftLayoutDirection:(int)imageEnum;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONTROLLER_PROTECTED_H_
