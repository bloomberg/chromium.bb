// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/side_swipe/side_swipe_controller.h"

#include <memory>

#import "base/ios/weak_nsobject.h"
#include "components/reading_list/core/reading_list_switches.h"
#import "components/reading_list/ios/reading_list_model.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/infobars/infobar_container_view.h"
#import "ios/chrome/browser/reading_list/reading_list_model_factory.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model_observer.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_side_swipe_provider.h"
#import "ios/chrome/browser/ui/side_swipe/card_side_swipe_view.h"
#import "ios/chrome/browser/ui/side_swipe/history_side_swipe_provider.h"
#import "ios/chrome/browser/ui/side_swipe/side_swipe_navigation_view.h"
#import "ios/chrome/browser/ui/side_swipe/side_swipe_util.h"
#import "ios/chrome/browser/ui/side_swipe_gesture_recognizer.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#import "ios/web/web_state/ui/crw_web_controller.h"

namespace ios_internal {
NSString* const kSideSwipeWillStartNotification =
    @"kSideSwipeWillStartNotification";
NSString* const kSideSwipeDidStopNotification =
    @"kSideSwipeDidStopNotification";
}  // namespace ios_internal

namespace {

enum class SwipeType { NONE, CHANGE_TAB, CHANGE_PAGE };

// Swipe starting distance from edge.
const CGFloat kSwipeEdge = 20;

// Distance between sections of iPad side swipe.
const CGFloat kIpadTabSwipeDistance = 100;

// Number of tabs to keep in the grey image cache.
const NSUInteger kIpadGreySwipeTabCount = 8;
}

@interface SideSwipeController ()<CRWWebStateObserver,
                                  TabModelObserver,
                                  UIGestureRecognizerDelegate> {
 @private

  base::WeakNSObject<TabModel> model_;

  // Side swipe view for tab navigation.
  base::scoped_nsobject<CardSideSwipeView> tabSideSwipeView_;

  // Side swipe view for page navigation.
  base::scoped_nsobject<SideSwipeNavigationView> pageSideSwipeView_;

  // YES if the user is currently swiping.
  BOOL inSwipe_;

  // Swipe gesture recognizer.
  base::scoped_nsobject<SideSwipeGestureRecognizer> swipeGestureRecognizer_;

  base::scoped_nsobject<SideSwipeGestureRecognizer> panGestureRecognizer_;

  // Used in iPad side swipe gesture, tracks the starting tab index.
  NSUInteger startingTabIndex_;

  // If the swipe is for a page change or a tab change.
  SwipeType swipeType_;

  // Bridge to observe the web state from Objective-C.
  std::unique_ptr<web::WebStateObserverBridge> webStateObserverBridge_;

  // Curtain over web view while waiting for it to load.
  base::scoped_nsobject<UIView> curtain_;

  // Provides forward/back action for history entries.
  base::scoped_nsobject<HistorySideSwipeProvider> historySideSwipeProvider_;

  // Provides forward action for reading list.
  base::scoped_nsobject<ReadingListSideSwipeProvider>
      readingListSideSwipeProvider_;

  base::WeakNSProtocol<id<SideSwipeContentProvider>> currentContentProvider_;
}

// Load grey snapshots for the next |kIpadGreySwipeTabCount| tabs in
// |direction|.
- (void)createGreyCache:(UISwipeGestureRecognizerDirection)direction;
// Tell snapshot cache to clear grey cache.
- (void)deleteGreyCache;
// Handle tab side swipe for iPad.  Change tabs according to swipe distance.
- (void)handleiPadTabSwipe:(SideSwipeGestureRecognizer*)gesture;
// Handle tab side swipe for iPhone. Introduces a CardSideSwipeView to convey
// the tab change.
- (void)handleiPhoneTabSwipe:(SideSwipeGestureRecognizer*)gesture;
// Overlays |curtain_| as a white view to hide the web view while it updates.
// Calls |completionHandler| when the curtain is removed.
- (void)addCurtainWithCompletionHandler:(ProceduralBlock)completionHandler;
// Removes the |curtain_| and calls |completionHandler| when the curtain is
// removed.
- (void)dismissCurtainWithCompletionHandler:(ProceduralBlock)completionHandler;
@end

@implementation SideSwipeController

@synthesize inSwipe = inSwipe_;
@synthesize swipeDelegate = swipeDelegate_;
@synthesize snapshotDelegate = snapshotDelegate_;

- (id)initWithTabModel:(TabModel*)model
          browserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(model);
  self = [super init];
  if (self) {
    model_.reset(model);
    [model_ addObserver:self];
    historySideSwipeProvider_.reset(
        [[HistorySideSwipeProvider alloc] initWithTabModel:model_]);

    if (reading_list::switches::IsReadingListEnabled()) {
      readingListSideSwipeProvider_.reset([[ReadingListSideSwipeProvider alloc]
          initWithReadingList:ReadingListModelFactory::GetForBrowserState(
                                  browserState)]);
    }
  }
  return self;
}

- (void)dealloc {
  [model_ removeObserver:self];
  [super dealloc];
}

- (void)addHorizontalGesturesToView:(UIView*)view {
  swipeGestureRecognizer_.reset([[SideSwipeGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handleSwipe:)]);
  [swipeGestureRecognizer_ setMaximumNumberOfTouches:1];
  [swipeGestureRecognizer_ setDelegate:self];
  [swipeGestureRecognizer_ setSwipeEdge:kSwipeEdge];
  [view addGestureRecognizer:swipeGestureRecognizer_];

  // Add a second gesture recognizer to handle swiping on the toolbar to change
  // tabs.
  panGestureRecognizer_.reset([[SideSwipeGestureRecognizer alloc]
      initWithTarget:self
              action:@selector(handlePan:)]);
  [panGestureRecognizer_ setMaximumNumberOfTouches:1];
  [panGestureRecognizer_ setSwipeThreshold:48];
  [panGestureRecognizer_ setDelegate:self];
  [view addGestureRecognizer:panGestureRecognizer_];
}

- (NSSet*)swipeRecognizers {
  return [NSSet setWithObjects:swipeGestureRecognizer_.get(), nil];
}

- (void)setEnabled:(BOOL)enabled {
  [swipeGestureRecognizer_ setEnabled:enabled];
}

- (BOOL)shouldAutorotate {
  return !([tabSideSwipeView_ window] || inSwipe_);
}

// Always return yes, as this swipe should work with various recognizers,
// including UITextTapRecognizer, UILongPressGestureRecognizer,
// UIScrollViewPanGestureRecognizer and others.
- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  return YES;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldBeRequiredToFailByGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  // Only take precedence over a pan gesture recognizer so that moving up and
  // down while swiping doesn't trigger overscroll actions.
  if ([otherGestureRecognizer isKindOfClass:[UIPanGestureRecognizer class]]) {
    return YES;
  }
  return NO;
}

// Gestures should only be recognized within |contentArea_| or the toolbar.
- (BOOL)gestureRecognizerShouldBegin:(SideSwipeGestureRecognizer*)gesture {
  if (inSwipe_) {
    return NO;
  }

  if ([swipeDelegate_ preventSideSwipe])
    return NO;

  CGPoint location = [gesture locationInView:gesture.view];

  // Both the toolbar frame and the contentView frame below are inset by
  // -1 because CGRectContainsPoint does include points on the max X and Y
  // edges, which will happen frequently with edge swipes from the right side.
  // Since the toolbar and the contentView can overlap, check the toolbar frame
  // first, and confirm the right gesture recognizer is firing.
  CGRect toolbarFrame =
      CGRectInset([[[swipeDelegate_ toolbarController] view] frame], -1, -1);
  if (CGRectContainsPoint(toolbarFrame, location)) {
    if (![gesture isEqual:panGestureRecognizer_]) {
      return NO;
    }

    if ([[swipeDelegate_ toolbarController] isOmniboxFirstResponder] ||
        [[swipeDelegate_ toolbarController] showingOmniboxPopup]) {
      return NO;
    }
    return YES;
  }

  // Otherwise, only allow contentView touches with |swipeGestureRecognizer_|.
  CGRect contentViewFrame =
      CGRectInset([[swipeDelegate_ contentView] frame], -1, -1);
  if (CGRectContainsPoint(contentViewFrame, location)) {
    if (![gesture isEqual:swipeGestureRecognizer_]) {
      return NO;
    }
    swipeType_ = SwipeType::CHANGE_PAGE;
    return YES;
  }
  return NO;
}

- (void)createGreyCache:(UISwipeGestureRecognizerDirection)direction {
  NSInteger dx = (direction == UISwipeGestureRecognizerDirectionLeft) ? -1 : 1;
  NSInteger index = startingTabIndex_ + dx;
  NSMutableArray* sessionIDs =
      [NSMutableArray arrayWithCapacity:kIpadGreySwipeTabCount];
  for (NSUInteger count = 0; count < kIpadGreySwipeTabCount; count++) {
    // Wrap around edges.
    if (index >= (NSInteger)[model_ count])
      index = 0;
    else if (index < 0)
      index = [model_ count] - 1;

    // Don't wrap past the starting index.
    if (index == (NSInteger)startingTabIndex_)
      break;

    Tab* tab = [model_ tabAtIndex:index];
    if (tab && tab.webController.usePlaceholderOverlay) {
      [sessionIDs addObject:tab.tabId];
    }
    index = index + dx;
  }
  [[SnapshotCache sharedInstance] createGreyCache:sessionIDs];
  for (Tab* tab in model_.get()) {
    tab.useGreyImageCache = YES;
  }
}

- (void)deleteGreyCache {
  [[SnapshotCache sharedInstance] removeGreyCache];
  for (Tab* tab in model_.get()) {
    tab.useGreyImageCache = NO;
  }
}

- (void)handlePan:(SideSwipeGestureRecognizer*)gesture {
  if (!IsIPadIdiom()) {
    return [self handleiPhoneTabSwipe:gesture];
  } else {
    return [self handleiPadTabSwipe:gesture];
  }
}

- (void)handleSwipe:(SideSwipeGestureRecognizer*)gesture {
  DCHECK(swipeType_ != SwipeType::NONE);
  if (swipeType_ == SwipeType::CHANGE_TAB) {
    if (!IsIPadIdiom()) {
      return [self handleiPhoneTabSwipe:gesture];
    } else {
      return [self handleiPadTabSwipe:gesture];
    }
  }
  if (swipeType_ == SwipeType::CHANGE_PAGE) {
    return [self handleSwipeToNavigate:gesture];
  }
  NOTREACHED();
}

- (void)handleiPadTabSwipe:(SideSwipeGestureRecognizer*)gesture {
  // Don't handle swipe when there are no tabs.
  NSInteger count = [model_ count];
  if (count == 0)
    return;

  if (gesture.state == UIGestureRecognizerStateBegan) {
    // If the toolbar is hidden, move it to visible.
    [[model_ currentTab] updateFullscreenWithToolbarVisible:YES];
    [[model_ currentTab] updateSnapshotWithOverlay:YES visibleFrameOnly:YES];
    [[NSNotificationCenter defaultCenter]
        postNotificationName:ios_internal::kSideSwipeWillStartNotification
                      object:nil];
    [[swipeDelegate_ tabStripController] setHighlightsSelectedTab:YES];
    startingTabIndex_ = [model_ indexOfTab:[model_ currentTab]];
    [self createGreyCache:gesture.direction];
  } else if (gesture.state == UIGestureRecognizerStateChanged) {
    // Side swipe for iPad involves changing the selected tab as the swipe moves
    // across the width of the view.  The screen is broken up into
    // |kIpadTabSwipeDistance| / |width| segments, with the current tab in the
    // first section.  The swipe does not wrap edges.
    CGFloat distance = [gesture locationInView:gesture.view].x;
    if (gesture.direction == UISwipeGestureRecognizerDirectionLeft) {
      distance = gesture.startPoint.x - distance;
    } else {
      distance -= gesture.startPoint.x;
    }

    NSInteger indexDelta = std::floor(distance / kIpadTabSwipeDistance);
    // Don't wrap past the first tab.
    if (indexDelta < count) {
      // Flip delta when swiping forward.
      if (IsSwipingForward(gesture.direction))
        indexDelta = 0 - indexDelta;

      Tab* currentTab = [model_ currentTab];
      NSInteger currentIndex = [model_ indexOfTab:currentTab];

      // Wrap around edges.
      NSInteger newIndex = (NSInteger)(startingTabIndex_ + indexDelta) % count;

      // C99 defines the modulo result as negative if our offset is negative.
      if (newIndex < 0)
        newIndex += count;

      if (newIndex != currentIndex) {
        Tab* tab = [model_ tabAtIndex:newIndex];
        // Toggle overlay preview mode for selected tab.
        [tab.webController setOverlayPreviewMode:YES];
        [model_ setCurrentTab:tab];
        // And disable overlay preview mode for last selected tab.
        [currentTab.webController setOverlayPreviewMode:NO];
      }
    }
  } else {
    if (gesture.state == UIGestureRecognizerStateCancelled) {
      Tab* tab = [model_ tabAtIndex:startingTabIndex_];
      [[model_ currentTab].webController setOverlayPreviewMode:NO];
      [model_ setCurrentTab:tab];
    }
    [[model_ currentTab].webController setOverlayPreviewMode:NO];

    // Redisplay the view if it was in overlay preview mode.
    [swipeDelegate_ displayTab:[model_ currentTab] isNewSelection:YES];
    [[swipeDelegate_ tabStripController] setHighlightsSelectedTab:NO];
    [self deleteGreyCache];
    [[NSNotificationCenter defaultCenter]
        postNotificationName:ios_internal::kSideSwipeDidStopNotification
                      object:nil];
  }
}

- (id<SideSwipeContentProvider>)contentProviderForGesture:(BOOL)goBack {
  if (goBack && [historySideSwipeProvider_ canGoBack]) {
    return historySideSwipeProvider_;
  }
  if (!goBack && [historySideSwipeProvider_ canGoForward]) {
    return historySideSwipeProvider_;
  }
  if (goBack && [readingListSideSwipeProvider_ canGoBack]) {
    return readingListSideSwipeProvider_;
  }
  if (!goBack && [readingListSideSwipeProvider_ canGoForward]) {
    return readingListSideSwipeProvider_;
  }
  return nil;
}

// Show swipe to navigate.
- (void)handleSwipeToNavigate:(SideSwipeGestureRecognizer*)gesture {
  if (gesture.state == UIGestureRecognizerStateBegan) {
    // If the toolbar is hidden, move it to visible.
    [[model_ currentTab] updateFullscreenWithToolbarVisible:YES];

    inSwipe_ = YES;
    [swipeDelegate_ updateAccessoryViewsForSideSwipeWithVisibility:NO];
    BOOL goBack = IsSwipingBack(gesture.direction);

    currentContentProvider_.reset([self contentProviderForGesture:goBack]);
    BOOL canNavigate = currentContentProvider_ != nil;

    CGRect gestureBounds = gesture.view.bounds;
    CGFloat headerHeight = [swipeDelegate_ headerHeight];
    CGRect navigationFrame =
        CGRectMake(CGRectGetMinX(gestureBounds),
                   CGRectGetMinY(gestureBounds) + headerHeight,
                   CGRectGetWidth(gestureBounds),
                   CGRectGetHeight(gestureBounds) - headerHeight);

    pageSideSwipeView_.reset([[SideSwipeNavigationView alloc]
        initWithFrame:navigationFrame
        withDirection:gesture.direction
          canNavigate:canNavigate
                image:[currentContentProvider_ paneIcon]
        rotateForward:[currentContentProvider_ rotateForwardIcon]]);
    [pageSideSwipeView_ setTargetView:[swipeDelegate_ contentView]];

    [gesture.view insertSubview:pageSideSwipeView_
                   belowSubview:[[swipeDelegate_ toolbarController] view]];
  }

  base::WeakNSObject<Tab> weakCurrentTab([model_ currentTab]);
  [pageSideSwipeView_ handleHorizontalPan:gesture
      onOverThresholdCompletion:^{
        BOOL wantsBack = IsSwipingBack(gesture.direction);
        web::WebState* webState = [weakCurrentTab webState];
        if (wantsBack) {
          [currentContentProvider_ goBack:webState];
        } else {
          [currentContentProvider_ goForward:webState];
        }

        if (webState && webState->IsLoading()) {
          webStateObserverBridge_.reset(
              new web::WebStateObserverBridge(webState, self));
          [self addCurtainWithCompletionHandler:^{
            inSwipe_ = NO;
          }];
        } else {
          inSwipe_ = NO;
        }
        [swipeDelegate_ updateAccessoryViewsForSideSwipeWithVisibility:YES];
      }
      onUnderThresholdCompletion:^{
        [swipeDelegate_ updateAccessoryViewsForSideSwipeWithVisibility:YES];
        inSwipe_ = NO;
      }];
}

// Show horizontal swipe stack view for iPhone.
- (void)handleiPhoneTabSwipe:(SideSwipeGestureRecognizer*)gesture {
  if (gesture.state == UIGestureRecognizerStateBegan) {
    // If the toolbar is hidden, move it to visible.
    [[model_ currentTab] updateFullscreenWithToolbarVisible:YES];

    inSwipe_ = YES;

    CGRect frame = [[swipeDelegate_ contentView] frame];

    // Add horizontal stack view controller.
    CGFloat headerHeight =
        [self.snapshotDelegate snapshotContentAreaForTab:[model_ currentTab]]
            .origin.y;
    if (tabSideSwipeView_) {
      [tabSideSwipeView_ setFrame:frame];
      [tabSideSwipeView_ setTopMargin:headerHeight];
    } else {
      tabSideSwipeView_.reset([[CardSideSwipeView alloc]
          initWithFrame:frame
              topMargin:headerHeight
                  model:model_]);
      [tabSideSwipeView_ setAutoresizingMask:UIViewAutoresizingFlexibleWidth |
                                             UIViewAutoresizingFlexibleHeight];
      [tabSideSwipeView_ setDelegate:swipeDelegate_];
      [tabSideSwipeView_ setBackgroundColor:[UIColor blackColor]];
    }

    // Ensure that there's an up-to-date snapshot of the current tab.
    [[model_ currentTab] updateSnapshotWithOverlay:YES visibleFrameOnly:YES];
    // Hide the infobar after snapshot has been updated (see the previous line)
    // to avoid it obscuring the cards in the side swipe view.
    [swipeDelegate_ updateAccessoryViewsForSideSwipeWithVisibility:NO];

    // Layout tabs with new snapshots in the current orientation.
    [tabSideSwipeView_
        updateViewsForDirection:gesture.direction
                    withToolbar:[swipeDelegate_ toolbarController]];

    // Insert behind infobar container (which is below toolbar)
    // so card border doesn't look janky during animation.
    DCHECK([swipeDelegate_ verifyToolbarViewPlacementInView:gesture.view]);
    // Insert above the toolbar.
    [gesture.view addSubview:tabSideSwipeView_];

    // Remove content area so it doesn't receive any pan events.
    [[swipeDelegate_ contentView] removeFromSuperview];
  }

  [tabSideSwipeView_ handleHorizontalPan:gesture];
}

- (void)addCurtainWithCompletionHandler:(ProceduralBlock)completionHandler {
  if (!curtain_) {
    curtain_.reset(
        [[UIView alloc] initWithFrame:[swipeDelegate_ contentView].bounds]);
    [curtain_ setBackgroundColor:[UIColor whiteColor]];
  }
  [[swipeDelegate_ contentView] addSubview:curtain_];

  // Fallback in case load takes a while. 3 seconds is a balance between how
  // long it can take a web view to clear the previous page image, and what
  // feels like to 'too long' to see the curtain.
  [self performSelector:@selector(dismissCurtainWithCompletionHandler:)
             withObject:[[completionHandler copy] autorelease]
             afterDelay:3];
}

- (void)resetContentView {
  CGRect frame = [swipeDelegate_ contentView].frame;
  frame.origin.x = 0;
  [swipeDelegate_ contentView].frame = frame;
}

- (void)dismissCurtainWithCompletionHandler:(ProceduralBlock)completionHandler {
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  webStateObserverBridge_.reset();
  [curtain_ removeFromSuperview];
  curtain_.reset();
  completionHandler();
}

#pragma mark - CRWWebStateObserver Methods

- (void)webStateDidStopLoading:(web::WebState*)webState {
  [self dismissCurtainWithCompletionHandler:^{
    inSwipe_ = NO;
  }];
}

#pragma mark - TabModelObserver Methods

- (void)tabModel:(TabModel*)model
    didChangeActiveTab:(Tab*)newTab
           previousTab:(Tab*)previousTab
               atIndex:(NSUInteger)index {
  // Toggling the gesture's enabled state off and on will effectively cancel
  // the gesture recognizer.
  [swipeGestureRecognizer_ setEnabled:NO];
  [swipeGestureRecognizer_ setEnabled:YES];
}

@end
