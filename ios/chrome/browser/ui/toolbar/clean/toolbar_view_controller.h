// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_positioner.h"
#import "ios/chrome/browser/ui/bubble/bubble_view_anchor_point_provider.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_element.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_consumer.h"

@protocol ApplicationCommands;
@protocol BrowserCommands;
@class ToolbarButtonFactory;
@class ToolbarButtonUpdater;
@class ToolbarToolsMenuButton;

// View controller for a toolbar, which will show a horizontal row of
// controls and/or labels.
@interface ToolbarViewController
    : UIViewController<ActivityServicePositioner,
                       BubbleViewAnchorPointProvider,
                       FullscreenUIElement,
                       ToolbarConsumer>

- (instancetype)initWithDispatcher:
                    (id<ApplicationCommands, BrowserCommands>)dispatcher
                     buttonFactory:(ToolbarButtonFactory*)buttonFactory
                     buttonUpdater:(ToolbarButtonUpdater*)buttonUpdater
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

// The dispatcher for this view controller.
@property(nonatomic, weak) id<ApplicationCommands, BrowserCommands> dispatcher;
// The location bar view, containing the omnibox.
@property(nonatomic, strong) UIView* locationBarView;
// The ToolsMenu button.
@property(nonatomic, strong, readonly) ToolbarToolsMenuButton* toolsMenuButton;
// Whether the toolbar is in the expanded state or not.
@property(nonatomic, assign) BOOL expanded;

// Adds the toolbar expanded state animations to |animator|, and changes the
// toolbar constraints in preparation for the animation.
- (void)addToolbarExpansionAnimations:(UIViewPropertyAnimator*)animator;
// Adds the toolbar contracted state animations to |animator|, and changes the
// toolbar constraints in preparation for the animation.
- (void)addToolbarContractionAnimations:(UIViewPropertyAnimator*)animator;
// Updates the view so a snapshot can be taken. It needs to be adapted,
// depending on if it is a snapshot displayed |onNTP| or not.
- (void)updateForSideSwipeSnapshotOnNTP:(BOOL)onNTP;
// Resets the view after taking a snapshot for a side swipe.
- (void)resetAfterSideSwipeSnapshot;
// Sets the background color of the Toolbar to the one of the Incognito NTP,
// with an |alpha|.
- (void)setBackgroundToIncognitoNTPColorWithAlpha:(CGFloat)alpha;
// Briefly animate the progress bar when a pre-rendered tab is displayed.
- (void)showPrerenderingAnimation;
// TODO(crbug.com/789583):Use named layout guide instead of frame.
// Returns visible omnibox frame in Toolbar's superview coordinate system.
- (CGRect)visibleOmniboxFrame;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_VIEW_CONTROLLER_H_
