// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/renderer_host/chrome_render_widget_host_view_mac_history_swiper.h"

#import "base/mac/sdk_forward_declarations.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#import "chrome/browser/ui/cocoa/history_overlay_controller.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace {
// The horizontal distance required to cause the browser to perform a history
// navigation.
const CGFloat kHistorySwipeThreshold = 0.08;

// The horizontal distance required for this class to start consuming events,
// which stops the events from reaching the renderer.
const CGFloat kConsumeEventThreshold = 0.01;

// If there has been sufficient vertical motion, the gesture can't be intended
// for history swiping.
const CGFloat kCancelEventVerticalThreshold = 0.24;

// If there has been sufficient vertical motion, and more vertical than
// horizontal motion, the gesture can't be intended for history swiping.
const CGFloat kCancelEventVerticalLowerThreshold = 0.01;

// Once we call `[NSEvent trackSwipeEventWithOptions:]`, we cannot reliably
// expect NSTouch callbacks. We set this variable to YES and ignore NSTouch
// callbacks.
BOOL forceMagicMouse = NO;
}  // namespace

@interface HistorySwiper ()
// Given a touch event, returns the average touch position.
- (NSPoint)averagePositionInEvent:(NSEvent*)event;

// Updates internal state with the location information from the touch event.
- (void)updateGestureCurrentPointFromEvent:(NSEvent*)event;

// Updates the state machine with the given touch event.
// Returns NO if no further processing of the event should happen.
- (BOOL)processTouchEventForHistorySwiping:(NSEvent*)event;

// Returns whether the wheel event should be consumed, and not passed to the
// renderer.
- (BOOL)shouldConsumeWheelEvent:(NSEvent*)event;
@end

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

- (void)rendererHandledWheelEvent:(const blink::WebMouseWheelEvent&)event
                         consumed:(BOOL)consumed {
  if (event.phase != NSEventPhaseBegan)
    return;
  beganEventUnconsumed_ = !consumed;
}

- (BOOL)canRubberbandLeft:(NSView*)view {
  Browser* browser = chrome::FindBrowserWithWindow([view window]);
  // If history swiping isn't possible, allow rubberbanding.
  if (!browser)
    return true;
  if (!chrome::CanGoBack(browser))
    return true;
  // History swiping is possible. By default, disallow rubberbanding.  If the
  // user has both started, and then cancelled history swiping for this
  // gesture, allow rubberbanding.
  return receivingTouches_ && recognitionState_ == history_swiper::kCancelled;
}

- (BOOL)canRubberbandRight:(NSView*)view {
  Browser* browser = chrome::FindBrowserWithWindow([view window]);
  // If history swiping isn't possible, allow rubberbanding.
  if (!browser)
    return true;
  if (!chrome::CanGoForward(browser))
    return true;
  // History swiping is possible. By default, disallow rubberbanding.  If the
  // user has both started, and then cancelled history swiping for this
  // gesture, allow rubberbanding.
  return receivingTouches_ && recognitionState_ == history_swiper::kCancelled;
}

- (void)beginGestureWithEvent:(NSEvent*)event {
  inGesture_ = YES;
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
  // Update the current point of the gesture.
  gestureCurrentPoint_ = [self averagePositionInEvent:event];

  // If the gesture doesn't have a start point, set one.
  if (!gestureStartPointValid_) {
    gestureStartPointValid_ = YES;
    gestureStartPoint_ = gestureCurrentPoint_;
  }
}

// Ideally, we'd set the gestureStartPoint_ here, but this method only gets
// called before the gesture begins, and the touches in an event are only
// available after the gesture begins.
- (void)touchesBeganWithEvent:(NSEvent*)event {
  receivingTouches_ = YES;
  ++currentGestureId_;

  // Reset state pertaining to previous gestures.
  gestureStartPointValid_ = NO;
  mouseScrollDelta_ = NSZeroSize;
  beganEventUnconsumed_ = NO;
  recognitionState_ = history_swiper::kPending;
}

- (void)touchesMovedWithEvent:(NSEvent*)event {
  [self processTouchEventForHistorySwiping:event];
}

- (void)touchesCancelledWithEvent:(NSEvent*)event {
  receivingTouches_ = NO;

  if (![self processTouchEventForHistorySwiping:event])
    return;

  [self cancelHistorySwipe];
}

- (void)touchesEndedWithEvent:(NSEvent*)event {
  receivingTouches_ = NO;

  if (![self processTouchEventForHistorySwiping:event])
    return;

  if (historyOverlay_) {
    BOOL finished = [self updateProgressBar];

    // If the gesture was completed, perform a navigation.
    if (finished)
      [self navigateBrowserInDirection:historySwipeDirection_];

    // Remove the history overlay.
    [self endHistorySwipe];
    // The gesture was completed.
    recognitionState_ = history_swiper::kCompleted;
  }
}

- (BOOL)processTouchEventForHistorySwiping:(NSEvent*)event {
  NSEventType type = [event type];
  if (type != NSEventTypeBeginGesture && type != NSEventTypeEndGesture &&
      type != NSEventTypeGesture) {
    return NO;
  }

  switch (recognitionState_) {
    case history_swiper::kCancelled:
    case history_swiper::kCompleted:
      return NO;
    case history_swiper::kPending:
      [self updateGestureCurrentPointFromEvent:event];
      return NO;
    case history_swiper::kPotential:
    case history_swiper::kTracking:
      break;
  }

  [self updateGestureCurrentPointFromEvent:event];

  // Consider cancelling the history swipe gesture.
  if ([self shouldCancelHorizontalSwipeWithCurrentPoint:gestureCurrentPoint_
                                             startPoint:gestureStartPoint_]) {
    [self cancelHistorySwipe];
    return NO;
  }

  if (recognitionState_ == history_swiper::kPotential) {
    // The user is in the process of doing history swiping.  If the history
    // swipe has progressed sufficiently far, stop sending events to the
    // renderer.
    BOOL sufficientlyFar = fabs(gestureCurrentPoint_.x - gestureStartPoint_.x) >
                           kConsumeEventThreshold;
    if (sufficientlyFar)
      recognitionState_ = history_swiper::kTracking;
  }

  if (historyOverlay_)
    [self updateProgressBar];
  return YES;
}

// Consider cancelling the horizontal swipe if the user was intending a
// vertical swipe.
- (BOOL)shouldCancelHorizontalSwipeWithCurrentPoint:(NSPoint)currentPoint
    startPoint:(NSPoint)startPoint {
  CGFloat yDelta = fabs(currentPoint.y - startPoint.y);
  CGFloat xDelta = fabs(currentPoint.x - startPoint.x);

  // The gesture is pretty clearly more vertical than horizontal.
  if (yDelta > 2 * xDelta)
    return YES;

  // There's been more vertical distance than horizontal distance.
  if (yDelta * 1.3 > xDelta && yDelta > kCancelEventVerticalLowerThreshold)
    return YES;

  // There's been a lot of vertical distance.
  if (yDelta > kCancelEventVerticalThreshold)
    return YES;

  return NO;
}

- (void)cancelHistorySwipe {
  [self endHistorySwipe];
  recognitionState_ = history_swiper::kCancelled;
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

  progress = (currentPoint.x - startPoint.x) / kHistorySwipeThreshold;
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
  historyOverlay_ = historyOverlay;

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

// We use an entirely different set of logic for magic mouse swipe events,
// since we do not get NSTouch callbacks.
- (BOOL)maybeHandleMagicMouseHistorySwiping:(NSEvent*)theEvent {
  // The 'trackSwipeEventWithOptions:' api doesn't handle momentum events.
  if ([theEvent phase] == NSEventPhaseNone)
    return NO;

  mouseScrollDelta_.width += [theEvent scrollingDeltaX];
  mouseScrollDelta_.height += [theEvent scrollingDeltaY];

  BOOL isHorizontalGesture =
    std::abs(mouseScrollDelta_.width) > std::abs(mouseScrollDelta_.height);
  if (!isHorizontalGesture)
    return NO;

  BOOL isRightScroll = [theEvent scrollingDeltaX] < 0;
  history_swiper::NavigationDirection direction =
      isRightScroll ? history_swiper::kForwards : history_swiper::kBackwards;
  BOOL browserCanMove =
      [self browserCanNavigateInDirection:direction event:theEvent];
  if (!browserCanMove)
    return NO;

  [self initiateMagicMouseHistorySwipe:isRightScroll event:theEvent];
  return YES;
}

- (void)initiateMagicMouseHistorySwipe:(BOOL)isRightScroll
                                 event:(NSEvent*)event {
  // Released by the tracking handler once the gesture is complete.
  __block HistoryOverlayController* historyOverlay =
      [[HistoryOverlayController alloc]
          initForMode:isRightScroll ? kHistoryOverlayModeForward
                                    : kHistoryOverlayModeBack];

  // The way this API works: gestureAmount is between -1 and 1 (float).  If
  // the user does the gesture for more than about 30% (i.e. < -0.3 or >
  // 0.3) and then lets go, it is accepted, we get a NSEventPhaseEnded,
  // and after that the block is called with amounts animating towards 1
  // (or -1, depending on the direction).  If the user lets go below that
  // threshold, we get NSEventPhaseCancelled, and the amount animates
  // toward 0.  When gestureAmount has reaches its final value, i.e. the
  // track animation is done, the handler is called with |isComplete| set
  // to |YES|.
  // When starting a backwards navigation gesture (swipe from left to right,
  // gestureAmount will go from 0 to 1), if the user swipes from left to
  // right and then quickly back to the left, this call can send
  // NSEventPhaseEnded and then animate to gestureAmount of -1. For a
  // picture viewer, that makes sense, but for back/forward navigation users
  // find it confusing. There are two ways to prevent this:
  // 1. Set Options to NSEventSwipeTrackingLockDirection. This way,
  //    gestureAmount will always stay > 0.
  // 2. Pass min:0 max:1 (instead of min:-1 max:1). This way, gestureAmount
  //    will become less than 0, but on the quick swipe back to the left,
  //    NSEventPhaseCancelled is sent instead.
  // The current UI looks nicer with (1) so that swiping the opposite
  // direction after the initial swipe doesn't cause the shield to move
  // in the wrong direction.
  forceMagicMouse = YES;
  [event trackSwipeEventWithOptions:NSEventSwipeTrackingLockDirection
      dampenAmountThresholdMin:-1
      max:1
      usingHandler:^(CGFloat gestureAmount,
                     NSEventPhase phase,
                     BOOL isComplete,
                     BOOL* stop) {
          if (phase == NSEventPhaseBegan) {
            [historyOverlay
                showPanelForView:[delegate_ viewThatWantsHistoryOverlay]];
            return;
          }

          BOOL ended = phase == NSEventPhaseEnded;

          // Dismiss the panel before navigation for immediate visual feedback.
          CGFloat progress = std::abs(gestureAmount) / 0.3;
          BOOL finished = progress >= 1.0;
          progress = MAX(0.0, progress);
          progress = MIN(1.0, progress);
          [historyOverlay setProgress:progress finished:finished];

          // |gestureAmount| obeys -[NSEvent isDirectionInvertedFromDevice]
          // automatically.
          Browser* browser =
              chrome::FindBrowserWithWindow(historyOverlay.view.window);
          if (ended && browser) {
            if (isRightScroll)
              chrome::GoForward(browser, CURRENT_TAB);
            else
              chrome::GoBack(browser, CURRENT_TAB);
          }

          if (ended || isComplete) {
            [historyOverlay dismiss];
            [historyOverlay release];
            historyOverlay = nil;
          }
      }];
}

// Checks if |theEvent| should trigger history swiping, and if so, does
// history swiping. Returns YES if the event was consumed or NO if it should
// be passed on to the renderer.
//
// There are 4 types of scroll wheel events:
// 1. Magic mouse swipe events.
//      These are identical to magic trackpad events, except that there are no
//      NSTouch callbacks.  The only way to accurately track these events is
//      with the  `trackSwipeEventWithOptions:` API. scrollingDelta{X,Y} is not
//      accurate over long distances (it is computed using the speed of the
//      swipe, rather than just the distance moved by the fingers).
// 2. Magic trackpad swipe events.
//      These are the most common history swipe events. Our logic is
//      predominantly designed to handle this use case.
// 3. Traditional mouse scrollwheel events.
//      These should not initiate scrolling. They can be distinguished by the
//      fact that `phase` and `momentumPhase` both return NSEventPhaseNone.
// 4. Momentum swipe events.
//      After a user finishes a swipe, the system continues to generate
//      artificial callbacks. `phase` returns NSEventPhaseNone, but
//      `momentumPhase` does not. Unfortunately, the callbacks don't work
//      properly (OSX 10.9). Sometimes, the system start sending momentum swipe
//      events instead of trackpad swipe events while the user is still
//      2-finger swiping.
- (BOOL)maybeHandleHistorySwiping:(NSEvent*)theEvent {
  if (![theEvent respondsToSelector:@selector(phase)])
    return NO;

  // The only events that this class consumes have type NSEventPhaseChanged.
  // This simultaneously weeds our regular mouse wheel scroll events, and
  // gesture events with incorrect phase.
  if ([theEvent phase] != NSEventPhaseChanged &&
      [theEvent momentumPhase] != NSEventPhaseChanged) {
    return NO;
  }

  // We've already processed this gesture.
  if (lastProcessedGestureId_ == currentGestureId_ &&
      recognitionState_ != history_swiper::kPending) {
    return [self shouldConsumeWheelEvent:theEvent];
  }

  // Don't allow momentum events to start history swiping.
  if ([theEvent momentumPhase] != NSEventPhaseNone)
    return NO;

  BOOL systemSettingsValid = [self systemSettingsAllowHistorySwiping:theEvent];
  if (!systemSettingsValid)
    return NO;

  if (![delegate_ shouldAllowHistorySwiping])
    return NO;

  // Don't enable history swiping until the renderer has decided to not consume
  // the event with phase NSEventPhaseBegan.
  if (!beganEventUnconsumed_)
    return NO;

  // Magic mouse and touchpad swipe events are identical except magic mouse
  // events do not generate NSTouch callbacks. Since we rely on NSTouch
  // callbacks to perform history swiping, magic mouse swipe events use an
  // entirely different set of logic.
  if ((inGesture_ && !receivingTouches_) || forceMagicMouse)
    return [self maybeHandleMagicMouseHistorySwiping:theEvent];

  // The scrollWheel: callback is only relevant if it happens while the user is
  // still actively using the touchpad.
  if (!receivingTouches_)
    return NO;

  // TODO(erikchen): Ideally, the direction of history swiping should not be
  // determined this early in a gesture, when it's unclear what the user is
  // intending to do. Since it is determined this early, make sure that there
  // is at least a minimal amount of horizontal motion.
  CGFloat xDelta = gestureCurrentPoint_.x - gestureStartPoint_.x;
  if (fabs(xDelta) < 0.001)
    return NO;

  BOOL isRightScroll = xDelta > 0;
  BOOL inverted = [self isEventDirectionInverted:theEvent];
  if (inverted)
    isRightScroll = !isRightScroll;

  history_swiper::NavigationDirection direction =
      isRightScroll ? history_swiper::kForwards : history_swiper::kBackwards;
  BOOL browserCanMove =
      [self browserCanNavigateInDirection:direction event:theEvent];
  if (!browserCanMove)
    return NO;

  lastProcessedGestureId_ = currentGestureId_;
  [self beginHistorySwipeInDirection:direction event:theEvent];
  recognitionState_ = history_swiper::kPotential;
  return [self shouldConsumeWheelEvent:theEvent];
}

- (BOOL)shouldConsumeWheelEvent:(NSEvent*)event {
  switch (recognitionState_) {
    case history_swiper::kPending:
    case history_swiper::kCancelled:
      return NO;
    case history_swiper::kTracking:
    case history_swiper::kCompleted:
      return YES;
    case history_swiper::kPotential:
      // It is unclear whether the user is attempting to perform history
      // swiping.  If the event has a vertical component, send it on to the
      // renderer.
      return event.scrollingDeltaY == 0;
  }
}
@end

@implementation HistorySwiper (PrivateExposedForTesting)
+ (void)resetMagicMouseState {
  forceMagicMouse = NO;
}
@end
