// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/renderer_host/chrome_render_widget_host_view_mac_history_swiper.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"

#import "base/mac/sdk_forward_declarations.h"
#import "chrome/browser/ui/cocoa/history_overlay_controller.h"

@implementation HistorySwiper
@synthesize delegate = delegate_;

- (id)initWithDelegate:(id<HistorySwiperDelegate>)delegate {
  self = [super init];
  if (self) {
    // Gesture ids start at 0.
    currentGestureId_ = 0;
    // No gestures have been processed
    lastProcessedGestureId_ = -1;
    delegate_ = delegate;
  }
  return self;
}

- (void)dealloc {
  [self endHistorySwipe];
  [super dealloc];
}

- (BOOL)handleEvent:(NSEvent*)event {
  if ([event type] == NSScrollWheel)
    return [self maybeHandleHistorySwiping:event];

  return NO;
}
- (void)gotUnhandledWheelEvent {
  gotUnhandledWheelEvent_ = YES;
}
- (void)scrollOffsetPinnedToLeft:(BOOL)left toRight:(BOOL)right {
  isPinnedLeft_ = left;
  isPinnedRight_ = right;
}

- (void)setHasHorizontalScrollbar:(BOOL)hasHorizontalScrollbar {
  hasHorizontalScrollbar_ = hasHorizontalScrollbar;
}

- (void)beginGestureWithEvent:(NSEvent*)event {
  inGesture_ = YES;
  ++currentGestureId_;
  // Reset state pertaining to previous gestures.
  historySwipeCancelled_ = NO;
  gestureStartPointValid_ = NO;
  gotUnhandledWheelEvent_ = NO;
}

- (void)endGestureWithEvent:(NSEvent*)event {
  inGesture_ = NO;
}

// This method assumes that there is at least 1 touch in the event.
// The event must correpond to a valid gesture, or else
// [NSEvent touchesMatchingPhase:inView:] will fail.
- (NSPoint)averagePositionInEvent:(NSEvent*)event {
  NSPoint position = NSMakePoint(0,0);
  int pointCount = 0;
  for (NSTouch* touch in
       [event touchesMatchingPhase:NSTouchPhaseAny inView:nil]) {
    position.x += touch.normalizedPosition.x;
    position.y += touch.normalizedPosition.y;
    ++pointCount;
  }

  if (pointCount > 1) {
    position.x /= pointCount;
    position.y /= pointCount;
  }

  return position;
}

- (void)updateGestureCurrentPointFromEvent:(NSEvent*)event {
  // The points in an event are not valid unless the event is part of
  // a gesture.
  if (inGesture_) {
    // Update the current point of the gesture.
    gestureCurrentPoint_ = [self averagePositionInEvent:event];

    // If the gesture doesn't have a start point, set one.
    if (!gestureStartPointValid_) {
      gestureStartPointValid_ = YES;
      gestureStartPoint_ = gestureCurrentPoint_;
    }
  }
}

// Ideally, we'd set the gestureStartPoint_ here, but this method only gets
// called before the gesture begins, and the touches in an event are only
// available after the gesture begins.
- (void)touchesBeganWithEvent:(NSEvent*)event {
  // Do nothing.
}

- (void)touchesMovedWithEvent:(NSEvent*)event {
  if (![self shouldProcessEventForHistorySwiping:event])
    return;

  [self updateGestureCurrentPointFromEvent:event];

  if (historyOverlay_) {
    // Consider cancelling the history swipe gesture.
    if ([self shouldCancelHorizontalSwipeWithCurrentPoint:gestureCurrentPoint_
        startPoint:gestureStartPoint_]) {
      [self cancelHistorySwipe];
      return;
    }

    [self updateProgressBar];
  }
}
- (void)touchesCancelledWithEvent:(NSEvent*)event {
  if (![self shouldProcessEventForHistorySwiping:event])
    return;

  if (historyOverlay_)
    [self cancelHistorySwipe];
}
- (void)touchesEndedWithEvent:(NSEvent*)event {
  if (![self shouldProcessEventForHistorySwiping:event])
    return;

  [self updateGestureCurrentPointFromEvent:event];
  if (historyOverlay_) {
    BOOL finished = [self updateProgressBar];

    // If the gesture was completed, perform a navigation.
    if (finished) {
      [self navigateBrowserInDirection:historySwipeDirection_];
    }

    // Remove the history overlay.
    [self endHistorySwipe];
  }
}

- (BOOL)shouldProcessEventForHistorySwiping:(NSEvent*)event {
  // TODO(erikchen): what is the point of NSEventTypeSwipe and NSScrollWheel?
  NSEventType type = [event type];
  return type == NSEventTypeBeginGesture || type == NSEventTypeEndGesture ||
      type == NSEventTypeGesture;
}

// Consider cancelling the horizontal swipe if the user was intending a
// vertical swipe.
- (BOOL)shouldCancelHorizontalSwipeWithCurrentPoint:(NSPoint)currentPoint
    startPoint:(NSPoint)startPoint {
  // There's been more vertical distance than horizontal distance.
  CGFloat yDelta = fabs(currentPoint.y - startPoint.y);
  CGFloat xDelta = fabs(currentPoint.x - startPoint.x);
  BOOL moreVertThanHoriz = yDelta > xDelta && yDelta > 0.1;

  // There's been a lot of vertical distance.
  BOOL muchVert = yDelta > 0.32;

  return moreVertThanHoriz || muchVert;
}

- (void)cancelHistorySwipe {
  [self endHistorySwipe];
  historySwipeCancelled_ = YES;
}

- (void)endHistorySwipe {
  [historyOverlay_ dismiss];
  [historyOverlay_ release];
  historyOverlay_ = nil;
}

// Returns whether the progress bar has been 100% filled.
- (BOOL)updateProgressBar {
  NSPoint currentPoint = gestureCurrentPoint_;
  NSPoint startPoint = gestureStartPoint_;

  float progress = 0;
  BOOL finished = NO;

  // This value was determined by experimentation.
  CGFloat requiredSwipeDistance = 0.08;
  progress = (currentPoint.x - startPoint.x) / requiredSwipeDistance;
  // If the swipe is a backwards gesture, we need to invert progress.
  if (historySwipeDirection_ == history_swiper::kBackwards)
    progress *= -1;
  // If the user has directions reversed, we need to invert progress.
  if (historySwipeDirectionInverted_)
    progress *= -1;

  if (progress >= 1.0)
    finished = YES;

  // Progress can't be less than 0 or greater than 1.
  progress = MAX(0.0, progress);
  progress = MIN(1.0, progress);

  [historyOverlay_ setProgress:progress finished:finished];

  return finished;
}

- (BOOL)isEventDirectionInverted:(NSEvent*)event {
  if ([event respondsToSelector:@selector(isDirectionInvertedFromDevice)])
    return [event isDirectionInvertedFromDevice];
  return NO;
}

// goForward indicates whether the user is starting a forward or backward
// history swipe.
// Creates and displays a history overlay controller.
// Responsible for cleaning up after itself when the gesture is finished.
// Responsible for starting a browser navigation if necessary.
// Does not prevent swipe events from propagating to other handlers.
- (void)beginHistorySwipeInDirection:
        (history_swiper::NavigationDirection)direction
                               event:(NSEvent*)event {
  // We cannot make any assumptions about the current state of the
  // historyOverlay_, since users may attempt to use multiple gesture input
  // devices simultaneously, which confuses Cocoa.
  [self endHistorySwipe];

  HistoryOverlayController* historyOverlay = [[HistoryOverlayController alloc]
      initForMode:(direction == history_swiper::kForwards)
                     ? kHistoryOverlayModeForward
                     : kHistoryOverlayModeBack];
  [historyOverlay showPanelForView:[delegate_ viewThatWantsHistoryOverlay]];
  historyOverlay_ = [historyOverlay retain];

  // Record whether the user was swiping forwards or backwards.
  historySwipeDirection_ = direction;
  // Record the user's settings.
  historySwipeDirectionInverted_ = [self isEventDirectionInverted:event];
}

- (BOOL)systemSettingsAllowHistorySwiping:(NSEvent*)event {
  if ([NSEvent
          respondsToSelector:@selector(isSwipeTrackingFromScrollEventsEnabled)])
    return [NSEvent isSwipeTrackingFromScrollEventsEnabled];
  return NO;
}

- (void)navigateBrowserInDirection:
            (history_swiper::NavigationDirection)direction {
  Browser* browser = chrome::FindBrowserWithWindow(
      historyOverlay_.view.window);
  if (browser) {
    if (direction == history_swiper::kForwards)
      chrome::GoForward(browser, CURRENT_TAB);
    else
      chrome::GoBack(browser, CURRENT_TAB);
  }
}

- (BOOL)browserCanNavigateInDirection:
        (history_swiper::NavigationDirection)direction
                                event:(NSEvent*)event {
  Browser* browser = chrome::FindBrowserWithWindow([event window]);
  if (!browser)
    return NO;

  if (direction == history_swiper::kForwards) {
    return chrome::CanGoForward(browser);
  } else {
    return chrome::CanGoBack(browser);
  }
}

// Checks if |theEvent| should trigger history swiping, and if so, does
// history swiping. Returns YES if the event was consumed or NO if it should
// be passed on to the renderer.
- (BOOL)maybeHandleHistorySwiping:(NSEvent*)theEvent {
  // We've already processed this gesture.
  if (lastProcessedGestureId_ == currentGestureId_) {
    // The user cancelled the history swiper. Ignore all events.
    if (historySwipeCancelled_)
      return NO;
    // The user completed the history swiper. Swallow all events.
    return YES;
  }

  BOOL systemSettingsValid = [self systemSettingsAllowHistorySwiping:theEvent];
  if (!systemSettingsValid)
    return NO;

  if (![delegate_ shouldAllowHistorySwiping])
    return NO;

  // Don't even consider enabling history swiping until blink has decided it is
  // not going to handle the event.
  if (!gotUnhandledWheelEvent_)
    return NO;

  if (![theEvent respondsToSelector:@selector(phase)])
    return NO;

  // If the window has a horizontal scroll bar, sometimes Cocoa gets confused
  // and sends us momentum scroll wheel events instead of gesture scroll events
  // (even though the user is still actively swiping).
  if ([theEvent phase] != NSEventPhaseChanged &&
      [theEvent momentumPhase] != NSEventPhaseChanged) {
    return NO;
  }

  if (!inGesture_)
    return NO;

  CGFloat yDelta = gestureCurrentPoint_.y - gestureStartPoint_.y;
  CGFloat xDelta = gestureCurrentPoint_.x - gestureStartPoint_.x;

  // Require the user's gesture to have moved more than a minimal amount.
  if (fabs(xDelta) < 0.01)
    return NO;

  // Require the user's gesture to be slightly more horizontal than vertical.
  BOOL isHorizontalGesture = fabs(xDelta) > 1.3 * fabs(yDelta);

  if (!isHorizontalGesture)
    return NO;

  BOOL isRightScroll = xDelta > 0;
  BOOL inverted = [self isEventDirectionInverted:theEvent];
  if (inverted)
    isRightScroll = !isRightScroll;

  if (isRightScroll) {
    if (hasHorizontalScrollbar_ && !isPinnedRight_)
      return NO;
  } else {
    if (hasHorizontalScrollbar_ && !isPinnedLeft_)
      return NO;
  }

  history_swiper::NavigationDirection direction =
      isRightScroll ? history_swiper::kForwards : history_swiper::kBackwards;
  BOOL browserCanMove =
      [self browserCanNavigateInDirection:direction event:theEvent];
  if (!browserCanMove)
    return NO;

  lastProcessedGestureId_ = currentGestureId_;
  [self beginHistorySwipeInDirection:direction event:theEvent];
  return YES;
}

@end
