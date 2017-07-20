// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TRANSITIONS_ANIMATORS_SWAP_FROM_ABOVE_ANIMATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TRANSITIONS_ANIMATORS_SWAP_FROM_ABOVE_ANIMATOR_H_

#import <UIKit/UIKit.h>

// An animation controller that animates in the new view controller from the top
// and animates out the old view controller to the top.
@interface SwapFromAboveAnimator
    : NSObject<UIViewControllerAnimatedTransitioning>
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TRANSITIONS_ANIMATORS_SWAP_FROM_ABOVE_ANIMATOR_H_
