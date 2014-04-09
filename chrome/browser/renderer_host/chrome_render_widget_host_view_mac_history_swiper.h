// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_WIDGET_HOST_VIEW_MAC_HISTORY_SWIPER_
#define CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_WIDGET_HOST_VIEW_MAC_HISTORY_SWIPER_

#import <Cocoa/Cocoa.h>

@class HistorySwiper;
@protocol HistorySwiperDelegate
// Return NO from this method is the view/render_widget_host should not
// allow history swiping.
- (BOOL)shouldAllowHistorySwiping;
// The history overlay is added to the view returning from this method.
- (NSView*)viewThatWantsHistoryOverlay;
@end

namespace history_swiper {

enum NavigationDirection {
  kBackwards = 0,
  kForwards,
};

enum GestureHandledState {
  kPending,    // Still waiting to determine whether the gesture was handled.
  kHandled,    // At least 1 event in the gesture was handled by blink.
  kUnhandled,  // No events so far have been handled by blink.
};

} // history_swiper

// Responsible for maintaining state for 2-finger swipe history navigation.
// Relevant blink/NSWindow touch events must be passed to this class.
// We want to be able to cancel history swipes if the user's swipe has a lot of
// vertical motion. The API [NSEvent trackSwipeEventWithOptions] doesn't give
// vertical swipe distance, and it swallows the touch events so that we can't
// independently gather them either. Instead of using that api, we manually
// track all touch events using the low level APIs touches*WithEvent:
@class HistoryOverlayController;
@interface HistorySwiper : NSObject {
 @private
  // Whether blink has handled the gesture.  This enum gets reset to kPending
  // whenever a new gesture starts.  History swiping is only enabled if blink
  // has never handled any of the events in the gesture.
  history_swiper::GestureHandledState gestureHandledState_;

  // This controller will exist if and only if the UI is in history swipe mode.
  HistoryOverlayController* historyOverlay_;
  // Each gesture received by the window is given a unique id.
  // The id is monotonically increasing.
  int currentGestureId_;
  // The location of the fingers when the gesture started.
  NSPoint gestureStartPoint_;
  // The current location of the fingers in the gesture.
  NSPoint gestureCurrentPoint_;
  // A flag that indicates that there is an ongoing gesture.
  // The method [NSEvent touchesMatchingPhase:inView:] is only valid for events
  // that are part of a gesture.
  BOOL inGesture_;
  // Each time a new gesture begins, we must get a new start point.
  // This ivar determines whether the start point is valid.
  int gestureStartPointValid_;
  // The id of the last gesture that we processed as a history swipe.
  int lastProcessedGestureId_;
  // A flag that indicates that we cancelled the history swipe for the current
  // gesture.
  BOOL historySwipeCancelled_;
  // A flag that indicates the user's intended direction with the history swipe.
  history_swiper::NavigationDirection historySwipeDirection_;
  // A flag that indicates whether the gesture has its direction inverted.
  BOOL historySwipeDirectionInverted_;

  id<HistorySwiperDelegate> delegate_;

  // Magic mouse and touchpad swipe events are identical except magic mouse
  // events do not generate NSTouch callbacks. Since we rely on NSTouch
  // callbacks to determine vertical scrolling, magic mouse swipe events use an
  // entirely different set of logic.
  //
  // The two event types do not play well together. Just calling the API
  // `[NSEvent trackSwipeEventWithOptions:]` will block touches{Began, Moved, *}
  // callbacks for a non-deterministic period of time (even after the swipe has
  // completed).
  BOOL receivedTouch_;
  // Cumulative scroll delta since scroll gesture start. Only valid during
  // scroll gesture handling. Used for history swiping.
  NSSize mouseScrollDelta_;
}

// Many event types are passed in, but the only one we care about is
// NSScrollWheel. We look at the phase to determine whether to trigger history
// swiping
- (BOOL)handleEvent:(NSEvent*)event;
- (void)gotWheelEventConsumed:(BOOL)consumed;

// The event passed in is a gesture event, and has touch data associated with
// the trackpad.
- (void)touchesBeganWithEvent:(NSEvent*)event;
- (void)touchesMovedWithEvent:(NSEvent*)event;
- (void)touchesCancelledWithEvent:(NSEvent*)event;
- (void)touchesEndedWithEvent:(NSEvent*)event;
- (void)beginGestureWithEvent:(NSEvent*)event;
- (void)endGestureWithEvent:(NSEvent*)event;

// These methods control whether a given view is allowed to rubberband in the
// given direction. This is inversely related to whether the view is allowed to
// 2-finger history swipe in the given direction.
- (BOOL)canRubberbandLeft:(NSView*)view;
- (BOOL)canRubberbandRight:(NSView*)view;

// Designated initializer.
- (id)initWithDelegate:(id<HistorySwiperDelegate>)delegate;

@property (nonatomic, assign) id<HistorySwiperDelegate> delegate;

@end

// Exposed only for unit testing, do not call directly.
@interface HistorySwiper (PrivateExposedForTesting)
+ (void)resetMagicMouseState;
@end

#endif // CHROME_BROWSER_RENDERER_HOST_CHROME_RENDER_WIDGET_HOST_VIEW_MAC_HISTORY_SWIPER_
