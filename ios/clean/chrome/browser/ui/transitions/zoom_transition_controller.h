// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TRANSITIONS_ZOOM_TRANSITION_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TRANSITIONS_ZOOM_TRANSITION_CONTROLLER_H_

#import <UIKit/UIKit.h>

// Transition delegate object that conforms to the
// UIViewControllerTransitioningDelegate protocol and provides a ZoomAnimator as
// animator for transitions.
@interface ZoomTransitionController
    : NSObject<UIViewControllerTransitioningDelegate>

// Optional object that can be passed into the animator to identify a specific
// location. For example, an object in a table or collection view might have
// its index path passed in so the delegate can map that to a screen location.
@property(nonatomic, copy) NSObject* presentationKey;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TRANSITIONS_ZOOM_TRANSITION_CONTROLLER_H_
