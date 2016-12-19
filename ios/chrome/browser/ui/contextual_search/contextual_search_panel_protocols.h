// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_PANEL_PROTOCOLS_H_
#define IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_PANEL_PROTOCOLS_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/contextual_search/panel_configuration.h"

namespace ContextualSearch {

typedef struct {
  PanelState state;  // state when the panel started moving, or current state if
                     // static.
  PanelState nextState;  // state panel is moving towards.
  CGFloat position;      // y-origin of top of panel in superview.
  CGFloat gradation;     // fractional progress to next state.
} PanelMotion;
}

@class ContextualSearchPanelView;

@protocol ContextualSearchPanelMotionObserver<NSObject>
@optional

// Called repeatedly as |panel| is moved. If |panel| is being animated,
// it will be called once from inside the animation block (so any view changes
// the observers make will also be animated).
// Use this call to handle any updates that must be made continuously as
// |panel| moves.
- (void)panel:(ContextualSearchPanelView*)panel
    didMoveWithMotion:(ContextualSearch::PanelMotion)motion;

// Called when a source of motion stops |panel| from moving. If |panel| was
// animated, this is called as part of the animation completion.
- (void)panel:(ContextualSearchPanelView*)panel
    didStopMovingWithMotion:(ContextualSearch::PanelMotion)motion;

// Called when |panel| has stopped moving and has changed state. If |panel|
// is animating to the new state, this is called *outside* of the animation
// block.
// |toState| will never be the same as |fromState|.
- (void)panel:(ContextualSearchPanelView*)panel
    didChangeToState:(ContextualSearch::PanelState)toState
           fromState:(ContextualSearch::PanelState)fromState;

// Called before |panel| will be used to animate a transition into a new tab.
// Observers who don't want to be notified of further panel motion activity
// should remove thmeselves at this point.
- (void)panelWillPromote:(ContextualSearchPanelView*)panel;

// Called as |panel| is animating into the position used for a new tab.
// Observers who need to make animated changes for this transition should do
// that in this method. |panel| will be destroyed shortly after this call is
// made; observers should expect no further calls after this.
- (void)panelIsPromoting:(ContextualSearchPanelView*)panel;

@end

// Protocol for a subview of a panel that has scrolling behavior that needs to
// interoperate with the panel's drag behavior.
@protocol ContextualSearchPanelScrollSynchronizer
// YES if the receiver has scrolled (e.g., its scroll offset is not {0,0}).
@property(nonatomic, readonly) BOOL scrolled;
// Gesture recognizer used by the receiver to detect scrolling.
@property(nonatomic, readonly) UIGestureRecognizer* scrollRecognizer;

@end

// Protocol for a an object that panel subview can forward panel-affecting
// events to.
@protocol ContextualSearchPanelTapHandler
// A panel subview was tapped with |gesture|; suitable for a gestrure
// recognizer's action method.
- (void)panelWasTapped:(UIGestureRecognizer*)gesture;
// A subview wants the panel to close.
- (void)closePanel;
@end

#endif  // IOS_CHROME_BROWSER_UI_CONTEXTUAL_SEARCH_CONTEXTUAL_SEARCH_PANEL_PROTOCOLS_H_
