// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/side_swipe/side_swipe_util.h"

#import <UIKit/UIKit.h>

#include "ios/chrome/browser/ui/rtl_geometry.h"

BOOL IsSwipingBack(UISwipeGestureRecognizerDirection direction) {
  if (UseRTLLayout())
    return direction == UISwipeGestureRecognizerDirectionLeft;
  else
    return direction == UISwipeGestureRecognizerDirectionRight;
}

BOOL IsSwipingForward(UISwipeGestureRecognizerDirection direction) {
  if (UseRTLLayout())
    return direction == UISwipeGestureRecognizerDirectionRight;
  else
    return direction == UISwipeGestureRecognizerDirectionLeft;
}
