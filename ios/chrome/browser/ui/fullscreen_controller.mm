// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen_controller.h"

#include <cmath>

#include "base/logging.h"

#import "ios/chrome/browser/ui/browser_view_controller.h"
#import "ios/chrome/browser/ui/history_popup/requirements/tab_history_constants.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/browser/ui/page_info/page_info_legacy_coordinator.h"
#import "ios/chrome/browser/ui/tabs/requirements/tab_strip_constants.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_controller_constants.h"
#import "ios/chrome/browser/ui/voice/voice_search_notification_names.h"
#include "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/ssl_status.h"
#import "ios/web/public/web_state/ui/crw_web_view_proxy.h"
#import "ios/web/web_state/ui/crw_web_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kSetupForTestingWillCloseAllTabsNotification =
    @"kSetupForTestingWillCloseAllTabsNotification";

using web::NavigationManager;

namespace {

class ScopedIncrementer {
 public:
  explicit ScopedIncrementer(int* value) : value_(value) { ++(*value_); }
  ~ScopedIncrementer() { --(*value_); }

 private:
  int* value_;
};

CGFloat kPrecision = 0.00001;

// Duration for the delay before showing the omnibox.
const double kShowOmniboxDelaySeconds = 0.5;
// Indicates if the FullScreenController returns nil from |init|. Used for
// testing purposes.
BOOL gEnabledForTests = YES;

// Compares that two CGFloat a and b are within a range of kPrecision of each
// other.
BOOL CGFloatEquals(CGFloat a, CGFloat b) {
  CGFloat delta = std::abs(a - b);

  return delta < kPrecision;
}

}  // anonymous namespace.

@interface FullScreenController ()<UIGestureRecognizerDelegate> {
  // Used to detect movement in the scrollview produced by this class.
  int selfTriggered_;
  // Used to detect if the keyboard is visible.
  BOOL keyboardIsVisible_;
  // Used to detect that the OverscrollActionsController is displaying its UI.
  // The FullScreenController is disabled when the OverscrollActionsController's
  // UI is displayed.
  BOOL overscrollActionsInProgress_;
  // Counter used to keep track of the number of actions currently disabling
  // full screen.
  uint fullScreenLock_;
  // CRWWebViewProxy object allows web view manipulations.
  id<CRWWebViewProxy> webViewProxy_;
}

// Access to the UIWebView's UIScrollView.
@property(weak, nonatomic, readonly) CRWWebViewScrollViewProxy* scrollViewProxy;
// The navigation controller of the page.
@property(nonatomic, readonly, assign) NavigationManager* navigationManager;
// The gesture recognizer set on the scrollview to detect tap. Must be readwrite
// for property releaser to work.
@property(nonatomic, readwrite, strong)
    UITapGestureRecognizer* userInteractionGestureRecognizer;
// The delegate responsible for providing the header height and moving the
// header.
@property(weak, nonatomic, readonly) id<FullScreenControllerDelegate> delegate;
// Current height of the header, in points. This is a pass-through method that
// fetches the header height from the FullScreenControllerDelegate.
@property(nonatomic, readonly) CGFloat headerHeight;
// |top| field of UIScrollView.contentInset value caused by header.
// Always 0 for WKWebView, as it does not support contentInset.
@property(nonatomic, readonly) CGFloat topContentInsetCausedByHeader;
// Last known y offset of the content in the scroll view during a scroll. Used
// to infer the direction of the current scroll.
@property(nonatomic, assign) CGFloat previousContentOffset;
// Last known y offset requested on the scroll view. In general the same value
// as previous content offset unless the offset was corrected by the controller
// to slide from under the toolbar.
@property(nonatomic, assign) CGFloat previousRequestedContentOffset;
// Whether or not the content of the scroll view fits entirely on screen when
// the toolbar is visible.
@property(nonatomic, readonly) BOOL contentFitsWithToolbarVisible;
// During a drag operation stores and remember the length of the latest scroll
// down operation. If a scroll up move happens later during the same gesture
// this will be used to delay the apparition of the header.
@property(nonatomic, assign) CGFloat lastScrollDownDistance;
// Tracks whether the current scrollview movements are triggered by the user or
// programmatically.
@property(nonatomic, assign) BOOL isUserTriggered;
// Tracks if fullscreen is currently disabled because of page load.
@property(nonatomic, assign) BOOL isFullScreenDisabledForLoading;
// Tracks if fullscreen is currently disabled because of unsecured page.
@property(nonatomic, readonly, assign) BOOL isFullScreenDisabledForSSLStatus;
// Tracks if fullscreen is currently disabled.
@property(nonatomic, readonly, assign) BOOL isFullScreenDisabled;
// Tracks if fullscreen is temporarily disabled for the current page.
@property(nonatomic, readonly, assign) BOOL isFullScreenDisabledTemporarily;
// Tracks if fullscreen is permanently disabled for the current page.
@property(nonatomic, readonly, assign) BOOL isFullScreenDisabledPermanently;
// Skip next attempt to correct the scroll offset for the toolbar height. This
// is necessary when programatically scrolling down the y offset.
@property(nonatomic, assign) BOOL skipNextScrollOffsetForHeader;
// Incremented each time a timed request to remove the header is sent,
// decremented when the timer fires. When it reach zero, the header is moved.
@property(nonatomic, assign) unsigned int delayedHideHeaderCount;
// ID of the session (each Tab represents a session).
@property(nonatomic, copy) NSString* sessionID;

// Returns if the given entry will be displayed with an error padlock. If this
// is the case, the toolbar should never be hidden on this entry.
- (BOOL)isEntryBrokenSSL:(web::NavigationItem*)item;
// Called at the start of a user scroll.
- (void)webViewScrollViewWillStartScrolling:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy;
// Called at the end of a scroll.
- (void)webViewScrollViewDidStopScrolling:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy;
// Called before and after the keyboard is appearing. Used to allow scroll
// events triggered by the keyboard appearing to go through.
- (void)keyboardStart:(NSNotification*)notification;
- (void)keyboardEnd:(NSNotification*)notification;
// Called before and after an action that disables full screen. The version
// resetting the timer will ensure that the header stay on screen for a little
// while.
- (void)incrementFullScreenLock;
- (void)decrementFullScreenLock;
// Called when the application is about to be the foreground application.
- (void)applicationWillEnterForeground:(NSNotification*)notification;
// Called from -webViewScrollViewDidScroll: Returns YES if the scroll should be
// ignored.
- (BOOL)shouldIgnoreScroll:(CRWWebViewScrollViewProxy*)webViewScrollViewProxy;
// Processes a scroll event triggered by a user action.
- (void)userTriggeredWebViewScrollViewDidScroll:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy;
// Processes a scroll event triggered by code (these could be initiated via
// Javascript, find in page or simply the keyboard sliding in and out).
- (void)codeTriggeredWebViewScrollViewDidScroll:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy;
// Returns YES if |scrollView_| is for the current tab.
- (BOOL)isScrollViewForCurrentTab;
// Shows the header. The header is hidden after kHideOmniboxDelaySeconds if the
// page requested fullscreen explicitly.
- (void)triggerHeader;
// Sets top inset to content view, and updates scroll view content offset to
// counteract the change in the content's view frame.
- (void)setContentViewTopContentPadding:(CGFloat)newTopInset;
// Hide the header if it is possible to do so.
- (void)hideHeaderIfPossible;
// Shows or hides the header as directed by |visible|. If necessary the delegate
// will be called synchronously with the desired offset and |animate| value.
// This method can be called when it is desirable to show or hide the header
// programmatically. It must be called when the header size changes.
- (void)moveHeaderToRestingPosition:(BOOL)visible animate:(BOOL)animate;
@end

@implementation FullScreenController

@synthesize delegate = delegate_;
@synthesize navigationManager = navigationManager_;
@synthesize previousContentOffset = previousContentOffset_;
@synthesize previousRequestedContentOffset = previousRequestedContentOffset_;
@synthesize lastScrollDownDistance = lastScrollDownDistance_;
@synthesize immediateDragDown = immediateDragDown_;
@synthesize isUserTriggered = userTriggered_;
@synthesize isFullScreenDisabledForLoading = isFullScreenDisabledForLoading_;
@synthesize skipNextScrollOffsetForHeader = skipNextScrollOffsetForHeader_;
@synthesize delayedHideHeaderCount = delayedHideHeaderCount_;
@synthesize sessionID = sessionID_;
@synthesize userInteractionGestureRecognizer =
    userInteractionGestureRecognizer_;

- (id)initWithDelegate:(id<FullScreenControllerDelegate>)delegate
     navigationManager:(NavigationManager*)navigationManager
             sessionID:(NSString*)sessionID {
  if (!gEnabledForTests)
    return nil;
  if ((self = [super init])) {
    DCHECK(sessionID);
    DCHECK(delegate);
    delegate_ = delegate;
    sessionID_ = [sessionID copy];
    navigationManager_ = navigationManager;

    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(keyboardStart:)
                   name:UIKeyboardWillShowNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(keyboardEnd:)
                   name:UIKeyboardWillHideNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(incrementFullScreenLock)
                   name:kMenuWillShowNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(decrementFullScreenLock)
                   name:kMenuWillHideNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(triggerHeader)
                   name:kWillStartTabStripTabAnimation
                 object:nil];
    [center addObserver:self
               selector:@selector(incrementFullScreenLock)
                   name:kTabHistoryPopupWillShowNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(decrementFullScreenLock)
                   name:kTabHistoryPopupWillHideNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(incrementFullScreenLock)
                   name:kVoiceSearchWillShowNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(decrementFullScreenLock)
                   name:kVoiceSearchWillHideNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(incrementFullScreenLock)
                   name:kVoiceSearchBarViewButtonSelectedNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(decrementFullScreenLock)
                   name:kVoiceSearchBarViewButtonDeselectedNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(applicationWillEnterForeground:)
                   name:UIApplicationWillEnterForegroundNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(triggerHeader)
                   name:kSetupForTestingWillCloseAllTabsNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(incrementFullScreenLock)
                   name:kPageInfoWillShowNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(decrementFullScreenLock)
                   name:kPageInfoWillHideNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(incrementFullScreenLock)
                   name:kLocationBarBecomesFirstResponderNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(decrementFullScreenLock)
                   name:kLocationBarResignsFirstResponderNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(incrementFullScreenLock)
                   name:kTabStripDragStarted
                 object:nil];
    [center addObserver:self
               selector:@selector(decrementFullScreenLock)
                   name:kTabStripDragEnded
                 object:nil];
    [center addObserver:self
               selector:@selector(incrementFullScreenLock)
                   name:kSideSwipeWillStartNotification
                 object:nil];
    [center addObserver:self
               selector:@selector(decrementFullScreenLock)
                   name:kSideSwipeDidStopNotification
                 object:nil];
    // TODO(crbug.com/451373): Evaluate using listeners instead of
    // notifications.
    [center addObserver:self
               selector:@selector(overscrollActionsWillStart)
                   name:kOverscrollActionsWillStart
                 object:nil];
    [center addObserver:self
               selector:@selector(overscrollActionsDidEnd)
                   name:kOverscrollActionsDidEnd
                 object:nil];
    [self moveHeaderToRestingPosition:YES];
  }
  return self;
}

- (void)invalidate {
  delegate_ = nil;
  navigationManager_ = NULL;
  [self.scrollViewProxy removeObserver:self];
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (CRWWebViewScrollViewProxy*)scrollViewProxy {
  return [webViewProxy_ scrollViewProxy];
}

- (CGFloat)headerHeight {
  return [self.delegate headerHeight];
}

- (CGFloat)topContentInsetCausedByHeader {
  if ([webViewProxy_ shouldUseInsetForTopPadding]) {
    // If the web view's |shouldUseInsetForTopPadding| is YES, fullscreen
    // header insets the content by modifying content inset.
    return self.headerHeight;
  }
  return 0.0f;
}

- (void)moveHeaderToRestingPosition:(BOOL)visible {
  [self moveHeaderToRestingPosition:visible animate:YES];
}

- (void)moveHeaderToRestingPosition:(BOOL)visible animate:(BOOL)animate {
  // If there is no delegate there is no need to do anything as the headerHeight
  // cannot be obtained.
  if (!self.delegate)
    return;
  DCHECK(visible || !self.isFullScreenDisabled);

  // The desired final position of the header.
  CGFloat headerPosition = visible ? 0.0 : self.headerHeight;

  // Check if there is anything to do.
  CGFloat delta = self.delegate.currentHeaderOffset - headerPosition;
  if (CGFloatEquals(delta, 0.0))
    return;

  // Do not further act on scrollview changes.
  ScopedIncrementer stack(&(self->selfTriggered_));

  // If the scrollview is not the current scrollview, don't update the UI.
  if (![self isScrollViewForCurrentTab])
    return;

  if (self.scrollViewProxy.contentOffset.y < 0.0 && delta < 0.0) {
    // If the delta is negative this means the header must be hidden more. Check
    // if the scrollview extents to the right place, there may be a need to
    // scroll it up.
    [self.delegate fullScreenController:self
               drawHeaderViewFromOffset:headerPosition
                         onWebViewProxy:webViewProxy_
                changeTopContentPadding:NO
                      scrollingToOffset:0.0f];
  } else {
    if (!visible && ![webViewProxy_ shouldUseInsetForTopPadding]) {
      // The header will be hidden, so if the content view is not using the
      // content inset, it is necessary to decrease the top padding, so more
      // content is visible to the user.
      CGFloat newTopContentPadding = self.headerHeight - headerPosition;
      CGFloat topContentPaddingChange =
          [webViewProxy_ topContentPadding] - newTopContentPadding;
      if (topContentPaddingChange <= self.scrollViewProxy.contentOffset.y) {
        // Padding can be decreased immediately and without animation as there
        // is enough content present behind the header.
        [self setContentViewTopContentPadding:newTopContentPadding];
      } else {
        // Header is taller that amount of hidden content, hence animated hide
        // is required.
        [self.delegate fullScreenController:self
                   drawHeaderViewFromOffset:headerPosition
                             onWebViewProxy:webViewProxy_
                    changeTopContentPadding:YES
                          scrollingToOffset:0.0f];
        return;
      }
    }
    // Only move the header, the content doesn't need to move.
    [self.delegate fullScreenController:self
               drawHeaderViewFromOffset:headerPosition
                                animate:animate];
  }
}

- (void)disableFullScreen {
  [self moveHeaderToRestingPosition:YES];
  self.isFullScreenDisabledForLoading = YES;
}

- (void)enableFullScreen {
  self.isFullScreenDisabledForLoading = NO;
}

- (void)shouldSkipNextScrollOffsetForHeader {
  self.skipNextScrollOffsetForHeader = YES;
}

- (void)moveContentBelowHeader {
  DCHECK(delegate_);
  DCHECK(webViewProxy_);
  [self moveHeaderToRestingPosition:YES animate:NO];
  CGPoint contentOffset = self.scrollViewProxy.contentOffset;
  contentOffset.y = 0;
  self.scrollViewProxy.contentOffset = contentOffset;
}

#pragma mark - private methods

- (BOOL)isEntryBrokenSSL:(web::NavigationItem*)item {
  if (!item)
    return NO;
  // Only BROKEN results in an error (vs. a warning); see toolbar_model_impl.cc.
  // TODO(qsr): Find a way to share this logic with the omnibox.
  const web::SSLStatus& ssl = item->GetSSL();
  switch (ssl.security_style) {
    case web::SECURITY_STYLE_UNKNOWN:
    case web::SECURITY_STYLE_UNAUTHENTICATED:
    case web::SECURITY_STYLE_AUTHENTICATED:
      return NO;
    case web::SECURITY_STYLE_AUTHENTICATION_BROKEN:
      return YES;
    default:
      NOTREACHED();
      return YES;
  }
}

- (BOOL)isFullScreenDisabled {
  return self.isFullScreenDisabledTemporarily ||
         self.isFullScreenDisabledPermanently;
}

- (BOOL)isFullScreenDisabledTemporarily {
  return fullScreenLock_ > 0 || self.isFullScreenDisabledForLoading;
}

- (BOOL)isFullScreenDisabledForSSLStatus {
  return self.navigationManager &&
         [self isEntryBrokenSSL:self.navigationManager->GetVisibleItem()];
}

- (BOOL)isFullScreenDisabledPermanently {
  return UIAccessibilityIsVoiceOverRunning() ||
         self.isFullScreenDisabledForSSLStatus ||
         CGRectIsEmpty(self.scrollViewProxy.frame);
}

- (void)hideHeaderIfPossible {
  // Covers a number of conditions, like a menu being up.
  if (self.isFullScreenDisabled)
    return;

  // Another FullScreenController is in control.
  if (![self isScrollViewForCurrentTab])
    return;

  // No autohide if the content needs to move.
  if (self.scrollViewProxy.contentOffset.y < 0.0)
    return;

  // It is quite safe to move the toolbar away.
  [self moveHeaderToRestingPosition:NO];
}

- (void)incrementFullScreenLock {
  // This method may be called late enough that it is unsafe to access the
  // delegate.
  fullScreenLock_++;
}

- (void)decrementFullScreenLock {
  // The corresponding notification for incrementing the lock may have been
  // posted before the FullScreenController was initialized. This can occur
  // when entering a URL or search query from the NTP since the CRWWebController
  // begins loading the page before the keyboard is dismissed.
  if (fullScreenLock_ > 0)
    fullScreenLock_--;
}

- (void)keyboardStart:(NSNotification*)notification {
  if (!keyboardIsVisible_) {
    keyboardIsVisible_ = YES;
    [self incrementFullScreenLock];
  }
  [self moveHeaderToRestingPosition:YES];
}

- (void)keyboardEnd:(NSNotification*)notification {
  if (keyboardIsVisible_) {
    keyboardIsVisible_ = NO;
    [self decrementFullScreenLock];
  }
}

- (void)applicationWillEnterForeground:(NSNotification*)notification {
  if (!self.isFullScreenDisabled && [self isScrollViewForCurrentTab]) {
    dispatch_time_t popTime = dispatch_time(
        DISPATCH_TIME_NOW, (int64_t)(kShowOmniboxDelaySeconds * NSEC_PER_SEC));
    dispatch_after(popTime, dispatch_get_main_queue(), ^{
      [self triggerHeader];
    });
  }
}

- (void)webViewScrollViewWillStartScrolling:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  self.isUserTriggered = YES;
  self.lastScrollDownDistance = 0.0;
}

- (void)webViewScrollViewDidStopScrolling:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  self.isUserTriggered = NO;
  // If an overscroll action is in progress, it means the header is already
  // shown, trying to reset its position would interfere with the
  // OverscrollActionsController.
  if (!overscrollActionsInProgress_) {
    CGFloat threshold = self.headerHeight / 2.0;

    BOOL visible = self.delegate.currentHeaderOffset < threshold ||
                   self.isFullScreenDisabled;
    [self moveHeaderToRestingPosition:visible];
  }
}

- (BOOL)shouldIgnoreScroll:(CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  if (overscrollActionsInProgress_)
    return YES;

  if (![self isScrollViewForCurrentTab])
    return YES;

  BOOL shouldIgnore = selfTriggered_ || webViewScrollViewProxy.isZooming ||
                      self.headerHeight == 0.0 || !self.delegate;

  if (self.isUserTriggered)
    return shouldIgnore;

  // Ignore simple realignment moves by 1 one pixel on retina display, called
  // sometimes at the end of an animation.
  CGFloat moveMagnitude = std::abs(self.previousContentOffset -
                                   webViewScrollViewProxy.contentOffset.y);
  shouldIgnore = shouldIgnore || moveMagnitude <= 0.5;

  // Never let the background show. The keyboard may sometimes center the
  // input fields in such a way that the inset of the scrollview is showing.
  // In those cases the header must be popped up unconditionally.
  CGFloat headerOffset = self.headerHeight - self.delegate.currentHeaderOffset;
  if (webViewScrollViewProxy.contentOffset.y + headerOffset < 0.0)
    shouldIgnore = NO;

  return shouldIgnore;
}

- (BOOL)contentFitsWithToolbarVisible {
  CGFloat viewportHeight = CGRectGetHeight(self.scrollViewProxy.frame) -
                           self.topContentInsetCausedByHeader;
  return self.scrollViewProxy.contentSize.height <= viewportHeight;
}

- (void)userTriggeredWebViewScrollViewDidScroll:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  // Calculate the relative move compared to the last checked position: positive
  // values are scroll up, negative are scroll down.
  CGFloat verticalDelta =
      webViewScrollViewProxy.contentOffset.y - self.previousContentOffset;

  // Scroll view is scrolled all the way to the top. Ignore the bouce up.
  BOOL isContentAtTop = webViewScrollViewProxy.contentOffset.y <=
                        -self.topContentInsetCausedByHeader;
  BOOL ignoreScrollAtContentTop = isContentAtTop && (0.0f < verticalDelta);

  // Scroll view is scrolled all the way to the bottom. Ignore the bounce down.
  // Also ignore the scroll up if the page is visible with the toolbar on-screen
  // as the toolbar should not be hidden in that case.
  BOOL ignoreScrollAtContentBottom =
      (webViewScrollViewProxy.contentOffset.y +
           webViewScrollViewProxy.frame.size.height >=
       webViewScrollViewProxy.contentSize.height) &&
      (verticalDelta < 0.0 || [self contentFitsWithToolbarVisible]);

  if (ignoreScrollAtContentTop || ignoreScrollAtContentBottom)
    verticalDelta = 0.0;

  if (!self.immediateDragDown) {
    // Accumulate or reset the lastScrollDownDistance. Scrolling up consumes
    // twice as fast as scrolling down accumulates.
    if (verticalDelta > 0.0)
      self.lastScrollDownDistance += verticalDelta;
    else
      self.lastScrollDownDistance += verticalDelta * 2.0;

    if (self.lastScrollDownDistance < 0.0)
      self.lastScrollDownDistance = 0.0;
  }

  // Changes the header offset and informs the delegate to perform the move.
  CGFloat newHeaderOffset = self.delegate.currentHeaderOffset;
  if (verticalDelta > 0.0 || webViewScrollViewProxy.contentOffset.y <= 0.0 ||
      self.lastScrollDownDistance <= 0.0) {
    newHeaderOffset += verticalDelta;
  }
  if (newHeaderOffset < 0.0)
    newHeaderOffset = 0.0;
  else if (newHeaderOffset > self.headerHeight)
    newHeaderOffset = self.headerHeight;

  [self.delegate fullScreenController:self
             drawHeaderViewFromOffset:newHeaderOffset
                              animate:NO];
}

- (void)codeTriggeredWebViewScrollViewDidScroll:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  if (webViewScrollViewProxy.contentOffset.y >= 0.0 && !keyboardIsVisible_)
    return;

  BOOL isFullyVisible = CGFloatEquals(self.delegate.currentHeaderOffset, 0.0);
  if (keyboardIsVisible_) {
    DCHECK(isFullyVisible);
    return;
  }

  CGFloat newOffset;
  if ([self contentFitsWithToolbarVisible] && !keyboardIsVisible_) {
    // Align the content just below the header if the scroll view's content fits
    // entirely on screen when the toolbar visible and if the keyboard is not
    // visible.
    // Note: The keyboard is visible when the user is editing a text field
    // at the bottom of the page and the page is scrolled to make it visible
    // for the user. Avoid changing the offset in this case.
    newOffset = -self.headerHeight;
  } else {
    newOffset = webViewScrollViewProxy.contentOffset.y;
    // Correct the offset to take into account the fact that the header is
    // obscuring the top of the view when scrolling down.
    if ((webViewScrollViewProxy.contentOffset.y <=
             self.previousRequestedContentOffset ||
         keyboardIsVisible_) &&
        !self.skipNextScrollOffsetForHeader)
      newOffset -= self.headerHeight;

    // Make sure the content is not too low.
    if (newOffset < -self.headerHeight)
      newOffset = -self.headerHeight;
  }

  if (isFullyVisible) {
    // As the header is already visible, just move the scrollview.
    webViewScrollViewProxy.contentOffset =
        CGPointMake(webViewScrollViewProxy.contentOffset.x, newOffset);
  }
}

- (BOOL)isScrollViewForCurrentTab {
  return [self.delegate isTabWithIDCurrent:self.sessionID];
}

- (void)triggerHeader {
  if (self.isFullScreenDisabled || ![self isScrollViewForCurrentTab])
    return;
  [self moveHeaderToRestingPosition:YES];
}

- (void)setContentViewTopContentPadding:(CGFloat)newTopPadding {
  [webViewProxy_ setTopContentPadding:newTopPadding];
}

- (void)setToolbarInsetsForHeaderOffset:(CGFloat)headerOffset {
  // Make space for the header in the scroll view.
  CGFloat topInset = self.headerHeight - headerOffset;
  UIEdgeInsets insets = self.scrollViewProxy.contentInset;
  insets.top = topInset;

  [self setContentViewTopContentPadding:topInset];
}

#pragma mark -
#pragma mark CRWWebControllerObserver methods

- (void)setWebViewProxy:(id<CRWWebViewProxy>)webViewProxy
             controller:(CRWWebController*)webController {
  DCHECK([webViewProxy scrollViewProxy]);
  webViewProxy_ = webViewProxy;
  [[webViewProxy scrollViewProxy] addObserver:self];
}

- (void)pageLoaded:(CRWWebController*)webController {
  [self enableFullScreen];
  web::WebState* webState = webController.webState;
  if (webState) {
    BOOL MIMETypeIsPDF = webState->GetContentsMimeType() == "application/pdf";
    [webViewProxy_ setShouldUseInsetForTopPadding:MIMETypeIsPDF];
  }
}

#pragma mark -
#pragma mark CRWWebViewScrollViewObserver

- (void)webViewScrollViewDidScroll:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  CGFloat previousRequestedContentOffset =
      webViewScrollViewProxy.contentOffset.y;
  if ([self shouldIgnoreScroll:webViewScrollViewProxy]) {
    // Do not act on those events, just record the eventual move.
    self.previousContentOffset = previousRequestedContentOffset;
    self.previousRequestedContentOffset = previousRequestedContentOffset;
    return;
  }

  // Ignore any scroll moves called recursively.
  ScopedIncrementer stack(&(self->selfTriggered_));

  if (self.isUserTriggered) {
    if (!self.isFullScreenDisabled)
      [self userTriggeredWebViewScrollViewDidScroll:webViewScrollViewProxy];
  } else {
    [self codeTriggeredWebViewScrollViewDidScroll:webViewScrollViewProxy];
  }
  self.previousContentOffset = webViewScrollViewProxy.contentOffset.y;
  self.previousRequestedContentOffset = previousRequestedContentOffset;
}
- (void)webViewScrollViewWillBeginDragging:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  [self webViewScrollViewWillStartScrolling:webViewScrollViewProxy];
}

- (void)webViewScrollViewDidEndDragging:
            (CRWWebViewScrollViewProxy*)webViewScrollViewProxy
                         willDecelerate:(BOOL)decelerate {
  DCHECK(self.delegate);
  if (!decelerate)
    [self webViewScrollViewDidStopScrolling:webViewScrollViewProxy];
}

- (void)webViewScrollViewDidEndScrollingAnimation:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  self.skipNextScrollOffsetForHeader = NO;
}

- (void)webViewScrollViewDidEndDecelerating:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  DCHECK(self.delegate);
  [self webViewScrollViewDidStopScrolling:webViewScrollViewProxy];
}

- (BOOL)webViewScrollViewShouldScrollToTop:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  if (webViewScrollViewProxy.contentInset.top != self.headerHeight) {
    // Move the toolbar first so the origin of the page moves down.
    [self moveHeaderToRestingPosition:YES];
  }
  return YES;
}

#pragma mark -
#pragma mark CRWWebViewScrollViewProxyObserver

- (void)webViewScrollViewProxyDidSetScrollView:
    (CRWWebViewScrollViewProxy*)webViewScrollViewProxy {
  webViewScrollViewProxy.contentOffset = CGPointMake(0.0, -self.headerHeight);
  [self setToolbarInsetsForHeaderOffset:0.0];
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)gestureRecognizer {
  return YES;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
    shouldRecognizeSimultaneouslyWithGestureRecognizer:
        (UIGestureRecognizer*)otherGestureRecognizer {
  // This is necessary for the gesture recognizer to receive all the touches.
  // If the default value of NO is returned the default recognizers on the
  // webview do take precedence.
  return YES;
}

- (BOOL)gestureRecognizer:(UIGestureRecognizer*)gestureRecognizer
       shouldReceiveTouch:(UITouch*)touch {
  return YES;
}

#pragma mark - Overscroll actions notifications handling

- (void)overscrollActionsWillStart {
  [self incrementFullScreenLock];
  overscrollActionsInProgress_ = YES;
}

- (void)overscrollActionsDidEnd {
  [self decrementFullScreenLock];
  overscrollActionsInProgress_ = NO;
}

#pragma mark - Used for testing

+ (void)setEnabledForTests:(BOOL)enabled {
  gEnabledForTests = enabled;
}

@end
