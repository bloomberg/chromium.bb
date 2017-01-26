// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERSCROLL_ACTIONS_OVERSCROLL_ACTIONS_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_OVERSCROLL_ACTIONS_OVERSCROLL_ACTIONS_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_view.h"
#import "ios/chrome/browser/ui/util/relaxed_bounds_constraints_hittest.h"
#import "ios/web/public/web_state/crw_web_controller_observer.h"
#import "ios/web/public/web_state/crw_web_view_scroll_view_proxy.h"

@class OverscrollActionsController;

// Describe the current state of the overscroll action controller.
enum class OverscrollState {
  NO_PULL_STARTED,  // No pull started.
  STARTED_PULLING,  // Started pulling.
  ACTION_READY      // Ready to take action on pull end.
};

// Notification sent when the overscroll actions controller will start
// displaying the UI.
extern NSString* const kOverscrollActionsWillStart;
// Notification sent when the overscroll actions controller did stop displaying
// the UI.
extern NSString* const kOverscrollActionsDidEnd;

// The delegate of the OverscrollActionsController, it provides the headerView
// on which the OverscrollActionsView will be added.
// The scrollView is used to control the state of the
// OverscrollActionsController.
// The delegate must implement the shouldAllowOverscrollActions method in order
// to allow overscroll actions.
// Finally the overscrollActionsController:didTriggerActionAtIndex: method is
// called when an action has been triggered.
@protocol OverscrollActionsControllerDelegate<NSObject>
// Called when an action has been triggered.
// The action index holds the current triggered action which are numbered left
// to right.
- (void)overscrollActionsController:(OverscrollActionsController*)controller
                   didTriggerAction:(OverscrollAction)action;
// Should return true when the delegate wants to enable the overscroll actions.
- (BOOL)shouldAllowOverscrollActions;
// The toolbar snapshot view that will be used to fade in/out the toolbar.
// This snapshot will be animated when performing the pull down animation
// revealing the actions.
- (UIView*)toolbarSnapshotView;
// The header view over which the overscroll action view will be added.
- (UIView<RelaxedBoundsConstraintsHitTestSupport>*)headerView;
// Called to retrieve the top inset added to the scrollview for the header.
- (CGFloat)overscrollActionsControllerHeaderInset:
    (OverscrollActionsController*)controller;
// Called to retrieve the current height of the header.
- (CGFloat)overscrollHeaderHeight;
@end

// The OverscrollActionsController controls the display of an
// OverscrollActionsView above the headerView provided by the delegate.
// Pulling down the scrollview will start showing the OverscrollActionView.
// Finally, the headerView frame will be resized in height during the process of
// showing the overscrollActionsView.
// The OverscrollActionsController can be used in two different modes depending
// on what kind of scrollview it's using:
// The first mode will use the scrollview provided by the
// CRWWebControllerObserver. To use this mode the OverscrollActionsController
// must be initialized with a nil scrollview. The -(instancetype)init
// initializer will do that for you.
// The second mode will use the scrollview provided during initialization and
// will ignore calls from the CRWWebControllerObserver.
// This second mode will typically be used in native tabs like the error tabs
// or the NTP.

@interface OverscrollActionsController
    : NSObject<CRWWebControllerObserver, UIScrollViewDelegate>

// Init the OverscrollActionsController with a nil scrollview.
// If the scrollview is nil then the OverscrollActionsController will be
// configured to use the scrollview from the CRWWebControllerObserver.
- (instancetype)init;
// Designated initializer, when initialized with a scrollview, the
// OverscrollActionsController will not use the CRWWebControllerObserver to
// configure the scrollview, it will directly use the passed scrollview.
- (instancetype)initWithScrollView:(UIScrollView*)scrollView;

// The scrollview the overscroll controller will control.
@property(nonatomic, readonly) UIScrollView* scrollView;
// The current state of the overscroll controller.
@property(nonatomic, assign, readonly) OverscrollState overscrollState;
// The delegate must be set for the OverscrollActionsController to work
// properly.
@property(nonatomic, assign) id<OverscrollActionsControllerDelegate> delegate;

// Used to clear state maintained by the controller and de-register from
// notifications. After this call the controller ceases to function and will
// clear its delegate.
- (void)invalidate;
// Schedules call to |invalidate| at the end of the current action. This lets
// the animation finish before invalidating the controller.
// If no action is running, calls |invalidate| immediately.
- (void)scheduleInvalidate;
// Force the controller to switch to NO_PULL_STARTED state.
- (void)clear;
// Disabling overscroll actions will stop showing the overscroll actions view on
// top of the header when the user is pulling the webview's scrollview.
// Disable/enableOverscrollActions method calls are ref counted, this means
// that calls to disable must be compensated by the same amount of call to
// enable in order to reenable overscroll actions.
- (void)disableOverscrollActions;
// Enabling overscroll actions will reverse the effect of a call to
// -disableOverscrollActions.
- (void)enableOverscrollActions;
// Sets the style of the overscroll actions.
- (void)setStyle:(OverscrollStyle)style;

@end

#endif  // IOS_CHROME_BROWSER_UI_OVERSCROLL_ACTIONS_OVERSCROLL_ACTIONS_CONTROLLER_H_
