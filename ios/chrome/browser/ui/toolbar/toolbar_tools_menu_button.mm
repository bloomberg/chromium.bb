// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/toolbar_tools_menu_button.h"

#import <QuartzCore/CAAnimation.h>
#import <QuartzCore/CAMediaTimingFunction.h>

#include "base/logging.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_button_tints.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The number of dots drawn.
const int kNumberOfDots = 3;
// Position of the topmost dot.
const CGFloat kDotOffsetX = 22;
const CGFloat kDotOffsetY = 18;
// Vertical space between dots.
const CGFloat kVerticalSpaceBetweenDots = 6;
// The duration of the animation, in seconds.
const CFTimeInterval kAnimationDuration = 1;
// The frame offset at which the animation to the intermediary value finishes.
const int kIntermediaryValueBeginFrame = 15;
// The frame offset at which the animation to the final value starts.
const int kIntermediaryValueEndFrame = 29;
// The frame offset at which the animation to the final value finishes.
const int kFinalValueBeginFrame = 37;
// The number of frames between the start of each dot's animation.
const double kFramesBetweenAnimationOfEachDot = 3;
// The maximum width of the segment/dots.
const CGFloat kMaxWidthOfSegment = 7.4;
// Constants for the properties of the stroke during the animations.
// The strokeEnd is slightly more than 0.5, because if the strokeEnd is
// exactly equal to strokeStart, the line is not drawn.
const CGFloat kStrokeStartAtRest = 0.5;
const CGFloat kStrokeEndAtRest = kStrokeStartAtRest + 0.001;
const CGFloat kLineWidthAtRest = 4;
const CGFloat kStrokeStartAtApogee = 0;
const CGFloat kStrokeEndAtApogee = 1;
const CGFloat kLineWidthAtApogee = 3;
}  // namespace

@interface ToolbarToolsMenuButton ()<CAAnimationDelegate> {
  // The style of the toolbar the button is in.
  ToolbarControllerStyle style_;
  // Whether the tools menu is visible.
  BOOL toolsMenuVisible_;
  // Whether the reading list contains unseen items.
  BOOL readingListContainsUnseenItems_;
  // The CALayers containing the drawn dots.
  NSMutableArray<CAShapeLayer*>* pathLayers_;
  // Whether the CALayers are being animated.
  BOOL animationOnGoing_;
}
// Updates the tint configuration based on the button's situation, e.g. whether
// the tools menu is visible or not.
- (void)updateTintOfButton;
// Initializes the pathLayers.
- (void)initializeShapeLayers;
// Returns a keyframe-based animation of the property identified by |keyPath|.
// The animation immediately sets the property's value to |initialValue|.
// After |frameStart| frames, the property's value animates to
// |intermediaryValue|, and then to |finalValue|.
- (CAAnimation*)animationWithInitialValue:(id)initialValue
                        intermediaryValue:(id)intermediaryValue
                               finalValue:(id)finalValue
                               frameStart:(int)frameStart
                               forKeyPath:(NSString*)keyPath;
// Starts animating the button towards the color |targetColor|.
- (void)animateToColor:(UIColor*)targetColor;
@end

@implementation ToolbarToolsMenuButton

- (instancetype)initWithFrame:(CGRect)frame
                        style:(ToolbarControllerStyle)style {
  if (self = [super initWithFrame:frame]) {
    style_ = style;
    pathLayers_ = [[NSMutableArray alloc] initWithCapacity:kNumberOfDots];

    [self setTintColor:toolbar::NormalButtonTint(style_)
              forState:UIControlStateNormal];
    [self setTintColor:toolbar::HighlighButtonTint(style_)
              forState:UIControlStateHighlighted];
  }
  return self;
}

- (void)setToolsMenuIsVisible:(BOOL)toolsMenuVisible {
  toolsMenuVisible_ = toolsMenuVisible;
  [self updateTintOfButton];
}

- (void)setReadingListContainsUnseenItems:(BOOL)readingListContainsUnseenItems {
  readingListContainsUnseenItems_ = readingListContainsUnseenItems;
  [self updateTintOfButton];
}

- (void)triggerAnimation {
  [self animateToColor:toolbar::HighlighButtonTint(style_)];
}

#pragma mark - Private

- (void)updateTintOfButton {
  if (toolsMenuVisible_ || readingListContainsUnseenItems_) {
    [self setTintColor:toolbar::HighlighButtonTint(style_)
              forState:UIControlStateNormal];
  } else {
    [self setTintColor:toolbar::NormalButtonTint(style_)
              forState:UIControlStateNormal];
  }
}

- (void)initializeShapeLayers {
  for (NSUInteger i = 0; i < pathLayers_.count; i++) {
    [pathLayers_[i] removeFromSuperlayer];
  }

  pathLayers_ = [[NSMutableArray alloc] initWithCapacity:kNumberOfDots];
  for (NSUInteger i = 0; i < kNumberOfDots; i++) {
    const CGFloat x = kDotOffsetX;
    const CGFloat y = kDotOffsetY + kVerticalSpaceBetweenDots * i;

    UIBezierPath* path = [UIBezierPath bezierPath];
    [path moveToPoint:CGPointMake(x - kMaxWidthOfSegment * 0.5, y)];
    [path addLineToPoint:CGPointMake(x + kMaxWidthOfSegment * 0.5, y)];

    CAShapeLayer* pathLayer = [CAShapeLayer layer];
    [pathLayer setFrame:self.bounds];
    [pathLayer setPath:path.CGPath];
    [pathLayer setStrokeColor:[self.tintColor CGColor]];
    [pathLayer setFillColor:nil];
    [pathLayer setLineWidth:kLineWidthAtRest];
    [pathLayer setLineCap:kCALineCapRound];
    [pathLayer setStrokeStart:kStrokeStartAtRest];
    [pathLayer setStrokeEnd:kStrokeEndAtRest];
    [self.layer addSublayer:pathLayer];
    [pathLayers_ addObject:pathLayer];
  }
}

- (CAAnimation*)animationWithInitialValue:(id)initialValue
                        intermediaryValue:(id)intermediaryValue
                               finalValue:(id)finalValue
                               frameStart:(int)frameStart
                               forKeyPath:(NSString*)keyPath {
  // The property is animated the following way:
  //
  //  value
  //    ^
  //    |                                             .final...final
  //    |                                            .
  //    |               .intermediary...intermediary.
  //    |              .
  // initial...initial.
  //    |
  //    +---------+-----------+--------------+-----------+-------+---> frames
  //    0       start         |              |           |       60
  //                        start+           |         start+
  //            kIntermediaryValueBeginFrame | kFinalValueBeginFrame
  //                                         |
  //                                       start+
  //                            kIntermediaryValueEndFrame

  CAKeyframeAnimation* animation =
      [CAKeyframeAnimation animationWithKeyPath:keyPath];
  animation.duration = kAnimationDuration;
  animation.removedOnCompletion = NO;

  // Set up the values.
  animation.values = @[
    initialValue, initialValue, intermediaryValue, intermediaryValue,
    finalValue, finalValue
  ];

  // Set up the timing functions.
  NSMutableArray* timings = [NSMutableArray array];
  for (size_t i = 0; i < [animation.values count] - 1; i++)
    [timings addObject:[CAMediaTimingFunction
                           functionWithName:kCAMediaTimingFunctionEaseIn]];
  animation.timingFunctions = timings;

  // Set up the key times.
  const double totalNumberOfFrames = 60 * kAnimationDuration;
  animation.keyTimes = @[
    @0, @(frameStart / totalNumberOfFrames),
    @((frameStart + kIntermediaryValueBeginFrame) / totalNumberOfFrames),
    @((frameStart + kIntermediaryValueEndFrame) / totalNumberOfFrames),
    @((frameStart + kFinalValueBeginFrame) / totalNumberOfFrames), @1
  ];

  DCHECK_EQ([animation.keyTimes count], [animation.values count]);
  DCHECK_EQ([animation.keyTimes count] - 1, [animation.timingFunctions count]);

#if DCHECK_IS_ON()
  for (NSNumber* number in animation.keyTimes) {
    DCHECK_GE([number floatValue], 0);
    DCHECK_LE([number floatValue], 1);
  }
#endif

  return animation;
}

- (void)animateToColor:(UIColor*)targetColor {
  animationOnGoing_ = YES;

  DCHECK(pathLayers_.count == kNumberOfDots);
  // Add four animations for each stroke.
  for (int i = 0; i < kNumberOfDots; i++) {
    CAShapeLayer* pathLayer = pathLayers_[i];
    const int frameStart =
        (kNumberOfDots - i) * kFramesBetweenAnimationOfEachDot;

    // Start of the stroke animation.
    CAAnimation* strokeStartAnimation =
        [self animationWithInitialValue:@(kStrokeStartAtRest)
                      intermediaryValue:@(kStrokeStartAtApogee)
                             finalValue:@(kStrokeStartAtRest)
                             frameStart:frameStart
                             forKeyPath:@"strokeStart"];

    // End of the stroke animation.
    CAAnimation* strokeEndAnimation =
        [self animationWithInitialValue:@(kStrokeEndAtRest)
                      intermediaryValue:@(kStrokeEndAtApogee)
                             finalValue:@(kStrokeEndAtRest)
                             frameStart:frameStart
                             forKeyPath:@"strokeEnd"];

    // Width of the stroke animation.
    CAAnimation* lineWidthAnimation =
        [self animationWithInitialValue:@(kLineWidthAtRest)
                      intermediaryValue:@(kLineWidthAtApogee)
                             finalValue:@(kLineWidthAtRest)
                             frameStart:frameStart
                             forKeyPath:@"lineWidth"];

    // Color of the stroke animation.
    CGColorRef initialColor = self.tintColor.CGColor;
    CGColorRef finalColor = targetColor.CGColor;
    CAAnimation* colorAnimation =
        [self animationWithInitialValue:(__bridge id)initialColor
                      intermediaryValue:(__bridge id)finalColor
                             finalValue:(__bridge id)finalColor
                             frameStart:frameStart
                             forKeyPath:@"strokeColor"];
    colorAnimation.fillMode = kCAFillModeForwards;

    // |self| needs to know when the animations are finished. This is achieved
    // by having |self| be registered as a CAAnimationDelegate.
    // Because all animations have the same duration, any animation can be used.
    // Arbitrarly use the |strokeStartAnimation| of the first dot.
    if (i == 0) {
      strokeStartAnimation.delegate = self;
    }

    [pathLayer addAnimation:strokeStartAnimation forKey:nil];
    [pathLayer addAnimation:strokeEndAnimation forKey:nil];
    [pathLayer addAnimation:lineWidthAnimation forKey:nil];
    [pathLayer addAnimation:colorAnimation forKey:nil];
  }

  self.tintColor = targetColor;
}

#pragma mark - UIView

- (void)tintColorDidChange {
  // The CAShapeLayer needs to be recreated when the tint color changes, to
  // reflect the tint color.
  // However, recreating the CAShapeLayer while it is animating cancels the
  // animation. To avoid canceling the animation, skip recreating the
  // CAShapeLayer when an animation is on going.
  // To reflect any potential tint color change, the CAShapeLayer will be
  // recreated at the end of the animation.
  if (!animationOnGoing_)
    [self initializeShapeLayers];
}

#pragma mark - CAAnimationDelegate

- (void)animationDidStop:(CAAnimation*)animation finished:(BOOL)flag {
  animationOnGoing_ = NO;
  // Recreate the CAShapeLayers in case the tint code changed while the
  // animation was going on.
  [self initializeShapeLayers];
}

@end
