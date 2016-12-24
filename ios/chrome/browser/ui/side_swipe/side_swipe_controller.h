// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SIDE_SWIPE_SIDE_SWIPE_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SIDE_SWIPE_SIDE_SWIPE_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/infobars/infobar_container_ios.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_snapshotting_delegate.h"
#import "ios/chrome/browser/ui/tabs/tab_strip_controller.h"
#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller.h"
#import "ios/web/web_state/ui/crw_swipe_recognizer_provider.h"

@class CardSideSwipeView;
@class SideSwipeGestureRecognizer;

namespace ios_internal {
// Notification sent when the user starts a side swipe (on tablet).
extern NSString* const kSideSwipeWillStartNotification;
// Notification sent when the user finishes a side swipe (on tablet).
extern NSString* const kSideSwipeDidStopNotification;
}  // namespace ios_internal

// A protocol for the Side Swipe controller sources.
@protocol SideSwipeContentProvider
// Returns whether this source can provide content for a back/forward side swipe
// gesture.
- (BOOL)canGoBack;
- (BOOL)canGoForward;

// Called on completion of a back/forward gesture.
- (void)goBack:(web::WebState*)webState;
- (void)goForward:(web::WebState*)webState;

// The icon to display in the side panel.
- (UIImage*)paneIcon;

// Whether the icon is oriented and should be reflected on forward pane.
- (BOOL)rotateForwardIcon;
@end

@protocol SideSwipeControllerDelegate
@required
// Called when the horizontal stack view is done and should be removed.
- (void)sideSwipeViewDismissAnimationDidEnd:(UIView*)sideSwipeView;
// Returns the main content view.
- (UIView*)contentView;
// Returns the toolbar controller.
- (WebToolbarController*)toolbarController;
// Returns the tabstrip controller.
- (TabStripController*)tabStripController;
// Makes |tab| the currently visible tab, displaying its view.  Calls
// -selectedTabChanged on the toolbar only if |newSelection| is YES.
- (void)displayTab:(Tab*)tab isNewSelection:(BOOL)newSelection;
// Check the invariant of "toolbar in front of infobar container which
// is in front of content area." This DCHECK happens if addSubview and/or
// insertSubview messed up the view ordering earlier.
- (BOOL)verifyToolbarViewPlacementInView:(UIView*)views;
// Controls the visibility of views such as the findbar, infobar and voice
// search bar.
- (void)updateAccessoryViewsForSideSwipeWithVisibility:(BOOL)visible;
// Returns the height of the header view for the tab model's current tab.
- (CGFloat)headerHeight;
// Returns |YES| if side swipe should be blocked from initiating, such as when
// voice search is up, or if the tools menu is enabled.
- (BOOL)preventSideSwipe;
@end

// Controls how an edge gesture is processed, either as tab change or a page
// change.  For tab changes two full screen CardSideSwipeView views are dragged
// across the screen. For page changes the SideSwipeControllerDelegate
// |contentView| is moved across the screen and a SideSwipeNavigationView is
// shown in the remaining space.
@interface SideSwipeController
    : NSObject<CRWSwipeRecognizerProvider, UIGestureRecognizerDelegate>

@property(nonatomic, assign) BOOL inSwipe;
@property(nonatomic, assign) id<SideSwipeControllerDelegate> swipeDelegate;
@property(nonatomic, assign) id<TabSnapshottingDelegate> snapshotDelegate;

// Initializer.
- (id)initWithTabModel:(TabModel*)model
          browserState:(ios::ChromeBrowserState*)browserState;

// Set up swipe gesture recognizers.
- (void)addHorizontalGesturesToView:(UIView*)view;

// Returns set of UIGestureRecognizer objects.
- (NSSet*)swipeRecognizers;

// Enable or disable the side swipe gesture recognizer.
- (void)setEnabled:(BOOL)enabled;

// Returns |NO| if the device should not rotate.
- (BOOL)shouldAutorotate;

// Resets the swipeDelegate's contentView frame origin x position to zero.
- (void)resetContentView;

@end

#endif  // IOS_CHROME_BROWSER_UI_SIDE_SWIPE_SIDE_SWIPE_CONTROLLER_H_
