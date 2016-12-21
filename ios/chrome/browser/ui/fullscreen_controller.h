// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/web/public/web_state/crw_web_controller_observer.h"
#import "ios/web/public/web_state/crw_web_view_scroll_view_proxy.h"

namespace ios_internal {
// Duration of the toolbar animation.
const NSTimeInterval kToolbarAnimationDuration = 0.3;
}  // namespace ios_internal

@class CRWWebViewScrollViewProxy;
@class FullScreenController;

namespace web {
class NavigationManager;
}

// Notification when the application is set up for testing.
extern NSString* const kSetupForTestingWillCloseAllTabsNotification;

@protocol FullScreenControllerDelegate<NSObject>

@required
// Called every time the header view needs to be moved in place according to
// the offset. The offset is a value between 0.0 (fully visible) and
// headerHeight (fully hidden). If animate is YES it is preferable for the
// delegate to slide the view in place instead of simply snapping it there. If
// animate is NO the view tracks touches on the screen and as such should be
// immediate.
- (void)fullScreenController:(FullScreenController*)fullscreenController
    drawHeaderViewFromOffset:(CGFloat)headerOffset
                     animate:(BOOL)animate;

// Called when there is a need to move the header in place and scroll the
// webViewProxy's scroll view at the same time. Should always be animated.
// Only happens during a call to -setHeaderHeight:visible:onScrollView:. If
// |changeTopContentPadding| is YES, then in addition to scrolling, delegate
// should also update webViewProxy's topContentPadding.
- (void)fullScreenController:(FullScreenController*)fullScreenController
    drawHeaderViewFromOffset:(CGFloat)headerOffset
              onWebViewProxy:(id<CRWWebViewProxy>)webViewProxy
     changeTopContentPadding:(BOOL)changeTopContentPadding
           scrollingToOffset:(CGFloat)contentOffset;

// Called to retrieve the current height of the header. Only called from
// -setHeaderVisible:, so that method needs to be explicitly called when the
// height changes.
- (CGFloat)headerHeight;

// Tests if the session ID matches the current tab.
- (BOOL)isTabWithIDCurrent:(NSString*)sessionID;

// Current offset of the header. A value between 0.0 (fully visible) and
// headerHeight (fully hidden).
- (CGFloat)currentHeaderOffset;

@end

// This class will track a scrollview to make a header disappear on scroll down
// and reappear on scroll up. This class expects the scrollview to have the
// FullScreenController instance set as an observer right after the call to
// -initWithDelegate:scrollView:
//
// It also assumes the header is a view rendering itself on top of the scroll
// view, the delegate will simply move it out of view as needed. The delegate is
// called every time the header view needs to be moved.
@interface FullScreenController
    : NSObject<CRWWebControllerObserver, CRWWebViewScrollViewProxyObserver>

// If set to YES this slightly alters the behaviour on drag down to pull the
// header to visible on the fist pixel moved. If set to NO (the default) there
// is a slight threshold before activating.
@property(nonatomic, assign) BOOL immediateDragDown;

// Designated initializer.
- (id)initWithDelegate:(id<FullScreenControllerDelegate>)delegate
     navigationManager:(web::NavigationManager*)navigationManager
             sessionID:(NSString*)sessionID;

// Used to clear state maintained by the controller and de-register from
// notifications. After this call the controller cease to function and will
// clear its delegate.
- (void)invalidate;

// Shows or hides the header as directed by |visible|. If necessary the delegate
// will be called synchronously with the desired offset and animate set to YES.
// This method can be called when it is desirable to show or hide the header
// programmatically. It must be called when the header size changes.
- (void)moveHeaderToRestingPosition:(BOOL)visible;

// Disabling full screen will pull the header to visible and keep it there no
// matter what the scrollview is doing.
- (void)disableFullScreen;
// Enabling fullscreen will reverse the effect of a call to -disableFullScreen.
// The toolbar will stay on screen until a move pushes it out.
- (void)enableFullScreen;

// Skip next attempt to correct the scroll offset for the toolbar height. This
// is necessary when programatically scrolling down the y offset.
- (void)shouldSkipNextScrollOffsetForHeader;

// Updates the insets during animation.
- (void)setToolbarInsetsForHeaderOffset:(CGFloat)headerOffset;

// Set the content offset of the underlying UIScrollView so that the content
// is not hidden by the header. The header will be moved to its visible position
// without animation if it is not already fully visible.
- (void)moveContentBelowHeader;
@end

@interface FullScreenController (UsedForTesting)
// Enables/Disables the FullScreenController in tests. The unit tests do not set
// the delegate which is crucial for methods to work on the controller.
// This a temporary solution.
// TODO(shreyasv): Find a better solution/remove this when FullScreenController
// moves to Tab.
+ (void)setEnabledForTests:(BOOL)enabled;
@end

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_CONTROLLER_H_
