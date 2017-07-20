// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TRANSITIONS_CONTAINMENT_TRANSITION_CONTEXT_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TRANSITIONS_CONTAINMENT_TRANSITION_CONTEXT_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/procedural_block_types.h"

// A transition context specific to child view controller containment
// transitions. For all other transitions (push/pop, present/dismiss, tab bar
// change), it's usually UIKit that provides such a context.  Since UIKit
// doesn't provide containment transitions API
// ContainmentTransitionContext is introduced.
//
// How to use it:
// When needing to animate in, animate out or swap child view controllers,
// create a context with the "from" and "to" view controllers, the common parent
// view controller, and the container view in which the transition animation
// will take stage. Then call |prepareTransitionWithAnimator:| before passing
// the context to an animation controller.
//
// Note that like with UIKit transitions API, the "from" view controller, if
// provided, needs to be a child of the parent. On the other side, the "to" view
// controller must not be a child already. It will be added automatically by the
// context when |startTransition| is called.
// Also, like with UIKit transitions API, it is necessary that the animation
// controller calls |completeTransition:| on the context. This will update the
// view controller hierarchy to reflect the transition.
@interface ContainmentTransitionContext
    : NSObject<UIViewControllerContextTransitioning>

// Instantiates a transition context for child view controller containment.
// The "from" and "to" can be nil (for animating in, animating out), but not at
// the same time.
- (instancetype)initWithFromViewController:(UIViewController*)fromViewController
                          toViewController:(UIViewController*)toViewController
                      parentViewController:
                          (UIViewController*)parentViewController
                                    inView:(UIView*)containerView
                                completion:(ProceduralBlockWithBool)completion;

// Prepare the process of transitioning between view controllers. It calls the
// appropriate |addChildViewController:| and |willMoveFromParentViewController:|
// required before animating their views.
// |animator| is optional and will only be used to call |animationEnded:| on it
// (if it responds to it) when the transition ends.
- (void)prepareTransitionWithAnimator:
    (id<UIViewControllerAnimatedTransitioning>)animator;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TRANSITIONS_CONTAINMENT_TRANSITION_CONTEXT_H_
