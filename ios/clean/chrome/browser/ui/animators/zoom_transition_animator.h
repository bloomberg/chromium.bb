// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_ANIMATORS_ZOOM_TRANSITION_ANIMATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_ANIMATORS_ZOOM_TRANSITION_ANIMATOR_H_

#import <UIKit/UIKit.h>

#import "ios/clean/chrome/browser/ui/animators/zoom_transition_delegate.h"

// A transition animator object. The transition (for presentation) will begin
// with the presented view occupying a rectangle supplied by the delegate, or
// defaulting to a square in the center of the presenter's view. The
// presentation animation will change the size of the rectangle to match the
// final presented size. For dismissal, the same animation is done in reverse.
@interface ZoomTransitionAnimator
    : NSObject<UIViewControllerAnimatedTransitioning>

// YES if the receiver is used for a presentation, NO (the default) if used
// for a dismissal. Calling code should set this before returning this object
// for UIKit to use.
@property(nonatomic, assign, getter=isPresenting) BOOL presenting;

// Optional object that can be passed into the delegate to identify a specific
// location. For example, an object in a table or collection view might have
// its index path passed in so the delegate can map that to a screen location.
@property(nonatomic, copy) NSObject* presentationKey;

// Delegate that can supply a source/destination rect for the animation.
@property(nonatomic, weak) id<ZoomTransitionDelegate> delegate;

// Convenience method to set a delegate from an array of objects that might
// implement the ZoomTransitionDelegate protocol. For example, either the
// source or presenting view controller (or neither) might implement the
// protocol. If |possibleDelegates| is empty, or if no object it contains
// conforms to ZoomTransitionDelegate, then the receiver's delegate will be
// nil. If multiple objects in |possibleDelegates| conforms to the protocol,
// then the first one will become the receiver's delegate.
- (void)selectDelegate:(NSArray<id<NSObject>>*)possibleDelegates;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_ANIMATORS_ZOOM_TRANSITION_ANIMATOR_H_
