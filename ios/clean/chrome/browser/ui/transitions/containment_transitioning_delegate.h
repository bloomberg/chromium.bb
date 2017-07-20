// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TRANSITIONS_CONTAINMENT_TRANSITIONING_DELEGATE_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TRANSITIONS_CONTAINMENT_TRANSITIONING_DELEGATE_H_

// Delegate protocol to retrieve an animation controller when performing a view
// controller containment animation.
@protocol ContainmentTransitioningDelegate

// Retrieves the animation controller to use when performing the transition from
// |removedChild| to |addedChild| in the parent view controller.
// |addedChild| may be nil, corresponding to just animating out |removedChild|.
// |removedChild| may be nil, corresponding to just animating in |addedChild|.
// |parent| is the view controller whose delegate is receiving this call.
- (id<UIViewControllerAnimatedTransitioning>)
animationControllerForAddingChildController:(UIViewController*)addedChild
                    removingChildController:(UIViewController*)removedChild
                               toController:(UIViewController*)parent;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TRANSITIONS_CONTAINMENT_TRANSITIONING_DELEGATE_H_
