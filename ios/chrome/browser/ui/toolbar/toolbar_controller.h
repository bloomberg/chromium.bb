// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_positioner.h"
#import "ios/chrome/browser/ui/bubble/bubble_view_anchor_point_provider.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_controller_constants.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_view.h"
#import "ios/chrome/browser/ui/tools_menu/tools_popup_controller.h"

@protocol ApplicationCommands;
@protocol BrowserCommands;
class ReadingListModel;
@class ToolsMenuConfiguration;

// Base class for a toolbar, containing the standard button set that is
// common across different types of toolbars and action handlers for those
// buttons (forwarding to the delegate). This is not intended to be used
// on its own, but to be subclassed by more specific toolbars that provide
// more buttons in the empty space.
@interface ToolbarController : UIViewController<ActivityServicePositioner,
                                                PopupMenuDelegate,
                                                BubbleViewAnchorPointProvider>

// The top-level toolbar view.
@property(nonatomic, strong) ToolbarView* view;
// The view containing all the content of the toolbar. It respects the trailing
// and leading anchors of the safe area.
@property(nonatomic, readonly, strong) UIView* contentView;
// The view for the toolbar background image. This is a subview of |view| to
// allow clients to alter the transparency of the background image without
// affecting the other components of the toolbar.
@property(nonatomic, readonly, strong) UIImageView* backgroundView;
// The view for the toolbar shadow image.  This is a subview of |view| to
// allow clients to alter the visibility of the shadow without affecting other
// components of the toolbar.
@property(nonatomic, readonly, strong) UIImageView* shadowView;

// The tools popup controller. Nil if the tools popup menu is not visible.
@property(nonatomic, readonly, strong)
    ToolsPopupController* toolsPopupController;

// Style of this toolbar.
@property(nonatomic, readonly, assign) ToolbarControllerStyle style;

// Returns the constraint controlling the height of the toolbar. If the
// constraint does not exist, creates it but does not activate it.
@property(nonatomic, readonly) NSLayoutConstraint* heightConstraint;

// The reading list model reflected by the toolbar.
@property(nonatomic, readwrite, assign) ReadingListModel* readingListModel;

// The command dispatcher this and any subordinate objects should use.
@property(nonatomic, readonly, weak) id<ApplicationCommands, BrowserCommands>
    dispatcher;

// Designated initializer.
//   |style| determines how the toolbar draws itself.
//   |dispatcher| is is the dispatcher for calling methods handled in other
//     parts of the app.
- (instancetype)initWithStyle:(ToolbarControllerStyle)style
                   dispatcher:
                       (id<ApplicationCommands, BrowserCommands>)dispatcher
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

// Height and Y offset to account for the status bar. Overridden by subclasses
// if the toolbar shouldn't extend through the status bar.
- (CGFloat)statusBarOffset;

// Called when the application has entered the background.
- (void)applicationDidEnterBackground:(NSNotification*)notify;

// Shows the tools popup menu.
- (void)showToolsMenuPopupWithConfiguration:
    (ToolsMenuConfiguration*)configuration;

// If |toolsPopupController_| is non-nil, dismisses the tools popup menu with
// animation.
- (void)dismissToolsMenuPopup;

// Sets the background to a particular alpha value. Intended for use by
// subcleasses that need to set the opacity of the entire toolbar.
- (void)setBackgroundAlpha:(CGFloat)alpha;

// Updates the tab stack button (if there is one) based on the given tab
// count. If |tabCount| > |kStackButtonMaxTabCount|, an easter egg is shown
// instead of the actual number of tabs.
- (void)setTabCount:(NSInteger)tabCount;

// Sets whether the share button is enabled or not.
- (void)setShareButtonEnabled:(BOOL)enabled;

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

// Shows/hides iPhone toolbar views for when the new tab page is displayed.
- (void)hideViewsForNewTabPage:(BOOL)hide;

// Performs the transition animation specified by |style|, animating the
// toolbar view from |beginFrame| to |endFrame|. Animations are added to
// subview depending on |style|:
//   - ToolbarTransitionStyleToStackView: faded out immediately
//   - ToolbarTransitionStyleToBVC: fade in from a vertical offset after a
//   delay
- (void)animateTransitionWithBeginFrame:(CGRect)beginFrame
                               endFrame:(CGRect)endFrame
                        transitionStyle:(ToolbarTransitionStyle)style;

// Reverses transition animations that are cancelled before they can finish.
- (void)reverseTransitionAnimations;

// Called when transition animations can be removed.
- (void)cleanUpTransitionAnimations;

// Triggers an animation on the tools menu button to draw the user's
// attention.
- (void)triggerToolsMenuButtonAnimation;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_CONTROLLER_H_
