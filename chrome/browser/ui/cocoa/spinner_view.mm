// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "spinner_view.h"

#import <QuartzCore/QuartzCore.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "skia/ext/skia_utils_mac.h"

namespace {
  const CGFloat k90_Degrees           = (M_PI / 2);
  const CGFloat k180_Degrees          = (M_PI);
  const CGFloat k270_Degrees          = (3 * M_PI / 2);
  const CGFloat k360_Degrees          = (2 * M_PI);
  const CGFloat kDesign_Width         = 28.0;
  const CGFloat kArc_Radius           = 12.5;
  const CGFloat kArc_Length           = 58.9;
  const CGFloat kArc_Stroke_Width     = 3.0;
  const CGFloat kArc_Animation_Time   = 1.333;
  const CGFloat kArc_Start_Angle      = k180_Degrees;
  const CGFloat kArc_End_Angle        = (kArc_Start_Angle + k270_Degrees);

  const SkColor kBlue   = SkColorSetRGB(66.0, 133.0, 244.0); // #4285f4.
  const SkColor kRed    = SkColorSetRGB(219.0, 68.0, 55.0);  // #db4437.
  const SkColor kYellow = SkColorSetRGB(244.0, 180.0, 0.0);  // #f4b400.
  const SkColor kGreen  = SkColorSetRGB(15.0, 157.0, 88.0);  // #0f9d58.
}

@interface SpinnerView()
{
  base::scoped_nsobject<CAAnimationGroup> spinner_animation_;
  CAShapeLayer* shape_layer_; //  Weak.
}
@end


@implementation SpinnerView

- (instancetype)initWithFrame:(NSRect)frame {
  if (self = [super initWithFrame:frame]) {
    [self setWantsLayer:YES];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

// Return a custom CALayer for the view (called from setWantsLayer:).
- (CALayer *)makeBackingLayer {
  CGRect bounds = [self bounds];
  // The spinner was designed to be |kDesign_Width| points wide. Compute the
  // scale factor needed to scale design parameters like |RADIUS| so that the
  // spinner scales to fit the view's bounds.
  CGFloat scale_factor = bounds.size.width / kDesign_Width;

  shape_layer_ = [CAShapeLayer layer];
  [shape_layer_ setBounds:bounds];
  [shape_layer_ setLineWidth:kArc_Stroke_Width * scale_factor];
  [shape_layer_ setLineCap:kCALineCapSquare];
  [shape_layer_ setLineDashPattern:[NSArray arrayWithObject:
      [NSNumber numberWithFloat:kArc_Length * scale_factor]]];
  [shape_layer_ setFillColor:NULL];

  // Create the arc that, when stroked, creates the spinner.
  base::ScopedCFTypeRef<CGMutablePathRef> shape_path(CGPathCreateMutable());
  CGPathAddArc(shape_path, NULL, bounds.size.width / 2.0,
               bounds.size.height / 2.0, kArc_Radius * scale_factor,
               kArc_Start_Angle, kArc_End_Angle, 0);
  [shape_layer_ setPath:shape_path];

  // Place |shape_layer_| in a parent layer so that it's easy to rotate
  // |shape_layer_| around the center of the view.
  CALayer* parent_layer = [CALayer layer];
  [parent_layer setBounds:bounds];
  [parent_layer addSublayer:shape_layer_];
  [shape_layer_ setPosition:CGPointMake(bounds.size.width / 2.0,
                                        bounds.size.height / 2.0)];

  return parent_layer;
}

// The spinner animation consists of four cycles that it continuously repeats.
// Each cycle consists of one complete rotation of the spinner's arc drawn in
// blue, red, yellow, or green. The arc's length also grows and shrinks over the
// course of each cycle, which the spinner achieves by drawing the arc using
// a (solid) dashed line pattern and animating the "lineDashPhase" property.
- (void)initializeAnimation {
  CGRect bounds = [self bounds];
  CGFloat scale_factor = bounds.size.width / kDesign_Width;

  // Create the first half of the arc animation, where it grows from a short
  // block to its full length.
  base::scoped_nsobject<CAMediaTimingFunction> timing_function(
      [[CAMediaTimingFunction alloc] initWithControlPoints:0.4 :0.0 :0.2 :1]);
  base::scoped_nsobject<CAKeyframeAnimation> first_half_animation(
      [[CAKeyframeAnimation alloc] init]);
  [first_half_animation setTimingFunction:timing_function];
  [first_half_animation setKeyPath:@"lineDashPhase"];
  NSMutableArray* animation_values = [NSMutableArray array];
  // Begin the lineDashPhase animation just short of the full arc length,
  // otherwise the arc will be zero length at start.
  [animation_values addObject:
      [NSNumber numberWithFloat:-(kArc_Length - 0.2) * scale_factor]];
  [animation_values addObject:[NSNumber numberWithFloat:0.0]];
  [first_half_animation setValues:animation_values];
  NSMutableArray* key_times = [NSMutableArray array];
  [key_times addObject:[NSNumber numberWithFloat:0.0]];
  [key_times addObject:[NSNumber numberWithFloat:1.0]];
  [first_half_animation setKeyTimes:key_times];
  [first_half_animation setDuration:kArc_Animation_Time / 2.0];
  [first_half_animation setRemovedOnCompletion:NO];
  [first_half_animation setFillMode:kCAFillModeForwards];

  // Create the second half of the arc animation, where it shrinks from full
  // length back to a short block.
  base::scoped_nsobject<CAKeyframeAnimation> second_half_animation(
      [[CAKeyframeAnimation alloc] init]);
  [second_half_animation setTimingFunction:timing_function];
  [second_half_animation setKeyPath:@"lineDashPhase"];
  animation_values = [NSMutableArray array];
  [animation_values addObject:[NSNumber numberWithFloat:0.0]];
  // Stop the lineDashPhase animation just before it reaches the full arc
  // length, otherwise the arc will be zero length at the end.
  [animation_values addObject:
      [NSNumber numberWithFloat:(kArc_Length - 0.3) * scale_factor]];
  [second_half_animation setValues:animation_values];
  [second_half_animation setKeyTimes:key_times];
  [second_half_animation setDuration:kArc_Animation_Time / 2.0];
  [second_half_animation setRemovedOnCompletion:NO];
  [second_half_animation setFillMode:kCAFillModeForwards];

  // Make four copies of the arc animations, to cover the four complete cycles
  // of the full animation.
  NSMutableArray* animations = [NSMutableArray array];
  NSUInteger i;
  CGFloat begin_time = 0;
  for (i = 0; i < 4; i++, begin_time += kArc_Animation_Time) {
    [first_half_animation setBeginTime:begin_time];
    [second_half_animation setBeginTime:begin_time + kArc_Animation_Time / 2.0];
    [animations addObject:first_half_animation];
    [animations addObject:second_half_animation];
    first_half_animation.reset([first_half_animation copy]);
    second_half_animation.reset([second_half_animation copy]);
  }

  // Create the rotation animation, which rotates the arc 360 degrees on each
  // cycle. The animation also includes a separate 90 degree rotation in the
  // opposite direction at the very end of each cycle. Ignoring the 360 degree
  // rotation, each arc starts as a short block at degree 0 and ends as a
  // short block at degree 270. Without a 90 degree rotation at the end of each
  // cycle, the short block would appear to suddenly jump from 270 degrees to
  // 360 degrees.
  CAKeyframeAnimation *rotation_animation = [CAKeyframeAnimation animation];
  [rotation_animation setTimingFunction:
      [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionLinear]];
  [rotation_animation setKeyPath:@"transform.rotation"];
  animation_values = [NSMutableArray array];
  // Use a key frame animation to rotate 360 degrees on each cycle, and then
  // jump back 90 degrees at the end of each cycle.
  [animation_values addObject:[NSNumber numberWithFloat:0.0]];
  [animation_values addObject:[NSNumber numberWithFloat:-1 * k360_Degrees]];
  [animation_values addObject:
      [NSNumber numberWithFloat:-1 * k360_Degrees + k90_Degrees]];
  [animation_values addObject:
      [NSNumber numberWithFloat:-2 * k360_Degrees + k90_Degrees]];
  [animation_values addObject:
      [NSNumber numberWithFloat:-2 * k360_Degrees + k180_Degrees]];
  [animation_values addObject:
      [NSNumber numberWithFloat:-3 * k360_Degrees + k180_Degrees]];
  [animation_values addObject:
      [NSNumber numberWithFloat:-3 * k360_Degrees + k270_Degrees]];
  [animation_values addObject:
      [NSNumber numberWithFloat:-4 * k360_Degrees + k270_Degrees]];
  [rotation_animation setValues:animation_values];
  key_times = [NSMutableArray array];
  [key_times addObject:[NSNumber numberWithFloat:0.0]];
  [key_times addObject:[NSNumber numberWithFloat:0.25]];
  [key_times addObject:[NSNumber numberWithFloat:0.25]];
  [key_times addObject:[NSNumber numberWithFloat:0.5]];
  [key_times addObject:[NSNumber numberWithFloat:0.5]];
  [key_times addObject:[NSNumber numberWithFloat:0.75]];
  [key_times addObject:[NSNumber numberWithFloat:0.75]];
  [key_times addObject:[NSNumber numberWithFloat:1.0]];
  [rotation_animation setKeyTimes:key_times];
  [rotation_animation setDuration:kArc_Animation_Time * 4.0];
  [rotation_animation setRemovedOnCompletion:NO];
  [rotation_animation setFillMode:kCAFillModeForwards];
  [rotation_animation setRepeatCount:HUGE_VALF];
  [animations addObject:rotation_animation];

  // Create a four-cycle-long key frame animation to transition between
  // successive colors at the end of each cycle.
  CAKeyframeAnimation *color_animation = [CAKeyframeAnimation animation];
  color_animation.timingFunction =
      [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionLinear];
  color_animation.keyPath = @"strokeColor";
  CGColorRef blueColor = gfx::CGColorCreateFromSkColor(kBlue);
  CGColorRef redColor = gfx::CGColorCreateFromSkColor(kRed);
  CGColorRef yellowColor = gfx::CGColorCreateFromSkColor(kYellow);
  CGColorRef greenColor = gfx::CGColorCreateFromSkColor(kGreen);
  animation_values = [NSMutableArray array];
  [animation_values addObject:(id)blueColor];
  [animation_values addObject:(id)blueColor];
  [animation_values addObject:(id)redColor];
  [animation_values addObject:(id)redColor];
  [animation_values addObject:(id)yellowColor];
  [animation_values addObject:(id)yellowColor];
  [animation_values addObject:(id)greenColor];
  [animation_values addObject:(id)greenColor];
  [animation_values addObject:(id)blueColor];
  [color_animation setValues:animation_values];
  CGColorRelease(blueColor);
  CGColorRelease(redColor);
  CGColorRelease(yellowColor);
  CGColorRelease(greenColor);
  key_times = [NSMutableArray array];
  // Begin the transition bewtween colors at T - 10% of the cycle.
  const CGFloat transition_offset = 0.1 * 0.25;
  [key_times addObject:[NSNumber numberWithFloat:0.0]];
  [key_times addObject:[NSNumber numberWithFloat:0.25 - transition_offset]];
  [key_times addObject:[NSNumber numberWithFloat:0.25]];
  [key_times addObject:[NSNumber numberWithFloat:0.50 - transition_offset]];
  [key_times addObject:[NSNumber numberWithFloat:0.5]];
  [key_times addObject:[NSNumber numberWithFloat:0.75 - transition_offset]];
  [key_times addObject:[NSNumber numberWithFloat:0.75]];
  [key_times addObject:[NSNumber numberWithFloat:0.999 - transition_offset]];
  [key_times addObject:[NSNumber numberWithFloat:0.999]];
  [color_animation setKeyTimes:key_times];
  [color_animation setDuration:kArc_Animation_Time * 4.0];
  [color_animation setRemovedOnCompletion:NO];
  [color_animation setFillMode:kCAFillModeForwards];
  [color_animation setRepeatCount:HUGE_VALF];
  [animations addObject:color_animation];

  // Use an animation group so that the animations are easier to manage, and to
  // give them the best chance of firing synchronously.
  CAAnimationGroup* group = [CAAnimationGroup animation];
  [group setDuration:kArc_Animation_Time * 4];
  [group setRepeatCount:HUGE_VALF];
  [group setFillMode:kCAFillModeForwards];
  [group setRemovedOnCompletion:NO];
  [group setAnimations:animations];

  spinner_animation_.reset([group retain]);
}

- (void)updateAnimation:(NSNotification*)notification {
  // Only animate the spinner if it's within a window, and that window is not
  // currently minimized or being minimized.
  if ([self window] && ![[self window] isMiniaturized] && ![self isHidden] &&
      ![[notification name] isEqualToString:
            NSWindowWillMiniaturizeNotification]) {
    if (spinner_animation_.get() == nil) {
      [self initializeAnimation];
    }
    // The spinner should never be animating at this point
    DCHECK(!is_animating_);
    if (!is_animating_) {
      [shape_layer_ addAnimation:spinner_animation_.get() forKey:nil];
      is_animating_ = true;
    }
  } else {
    [shape_layer_ removeAllAnimations];
    is_animating_ = false;
  }
}

// Register/unregister for window miniaturization event notifications so that
// the spinner can stop animating if the window is minaturized
// (i.e. not visible).
- (void)viewWillMoveToWindow:(NSWindow*)newWindow {
  if ([self window]) {
    [[NSNotificationCenter defaultCenter]
        removeObserver:self
                  name:NSWindowWillMiniaturizeNotification
                object:[self window]];
    [[NSNotificationCenter defaultCenter]
        removeObserver:self
                  name:NSWindowDidDeminiaturizeNotification
                object:[self window]];
  }

  if (newWindow) {
    [[NSNotificationCenter defaultCenter]
        addObserver:self
     selector:@selector(updateAnimation:)
               name:NSWindowWillMiniaturizeNotification
             object:newWindow];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
     selector:@selector(updateAnimation:)
               name:NSWindowDidDeminiaturizeNotification
             object:newWindow];
  }
}

// Start or stop the animation whenever the view is added to or removed from a
// window.
- (void)viewDidMoveToWindow {
  [self updateAnimation:nil];
}

// Start or stop the animation whenever the view is unhidden or hidden.
- (void)setHidden:(BOOL)flag
{
  [super setHidden:flag];
  [self updateAnimation:nil];
}


@end
