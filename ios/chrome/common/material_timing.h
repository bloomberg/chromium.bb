// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_MATERIAL_TIMING_H_
#define IOS_CHROME_COMMON_MATERIAL_TIMING_H_

#import <CoreGraphics/CoreGraphics.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>

namespace ios {
namespace material {

extern const CGFloat kDuration0;
extern const CGFloat kDuration1;
extern const CGFloat kDuration2;
extern const CGFloat kDuration3;
extern const CGFloat kDuration4;
extern const CGFloat kDuration5;
extern const CGFloat kDuration6;
extern const CGFloat kDuration7;
extern const CGFloat kDuration8;

// Type of material timing curve.
typedef NS_ENUM(NSUInteger, Curve) {
  CurveEaseInOut,
  CurveEaseOut,
  CurveEaseIn,
  CurveLinear,
};

// Per material spec, a motion curve with "follow through".
CAMediaTimingFunction* TransformCurve2();

// Returns a timing funtion related to the given |curve|.
CAMediaTimingFunction* TimingFunction(Curve curve);

// Performs a standard UIView animation using a material timing |curve|.
// Note: any curve option specified in |options| will be ignored in favor of the
// specified curve value.
// See also: +[UIView animateWithDuration:delay:animations:completion].
void Animate(NSTimeInterval duration,
             NSTimeInterval delay,
             Curve curve,
             UIViewAnimationOptions options,
             void (^animations)(void),
             void (^completion)(BOOL));

// Performs a standard UIView transition animation using a material timing
// |curve|.
// Note: any curve option specified in |options| will be ignored in favor of the
// specified curve value.
// See also:
// +[UIView transitionWithView:duration:options:animations:completion].
void Transition(UIView* view,
                NSTimeInterval duration,
                Curve curve,
                UIViewAnimationOptions options,
                void (^animations)(void),
                void (^completion)(BOOL));

}  // material
}  // ios

#endif  // IOS_CHROME_COMMON_MATERIAL_TIMING_H_
