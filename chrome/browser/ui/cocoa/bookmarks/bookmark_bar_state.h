// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_BAR_STATE_H_
#define CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_BAR_STATE_H_
#pragma once

#import <Cocoa/Cocoa.h>

namespace bookmarks {

// States for the bookmark bar.
enum VisualState {
  kInvalidState  = 0,
  kHiddenState   = 1,
  kShowingState  = 2,
  kDetachedState = 3,
};

}  // namespace bookmarks

// The interface for controllers (etc.) which can give information about the
// bookmark bar's state.
@protocol BookmarkBarState

// Returns YES if the bookmark bar is currently visible (as a normal toolbar or
// as a detached bar on the NTP), NO otherwise.
- (BOOL)isVisible;

// Returns YES if an animation is currently running, NO otherwise.
- (BOOL)isAnimationRunning;

// Returns YES if the bookmark bar is in the given state and not in an
// animation, NO otherwise.
- (BOOL)isInState:(bookmarks::VisualState)state;

// Returns YES if the bookmark bar is animating from the given state (to any
// other state), NO otherwise.
- (BOOL)isAnimatingToState:(bookmarks::VisualState)state;

// Returns YES if the bookmark bar is animating to the given state (from any
// other state), NO otherwise.
- (BOOL)isAnimatingFromState:(bookmarks::VisualState)state;

// Returns YES if the bookmark bar is animating from the first given state to
// the second given state, NO otherwise.
- (BOOL)isAnimatingFromState:(bookmarks::VisualState)fromState
                     toState:(bookmarks::VisualState)toState;

// Returns YES if the bookmark bar is animating between the two given states (in
// either direction), NO otherwise.
- (BOOL)isAnimatingBetweenState:(bookmarks::VisualState)fromState
                       andState:(bookmarks::VisualState)toState;

// Returns how morphed into the detached bubble the bookmark bar should be (1 =
// completely detached, 0 = normal).
- (CGFloat)detachedMorphProgress;

@end

#endif  // CHROME_BROWSER_UI_COCOA_BOOKMARKS_BOOKMARK_BAR_STATE_H_
