// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/toolbar_view.h"

#import "ios/chrome/browser/ui/toolbar/toolbar_controller_base_feature.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_frame_delegate.h"

@implementation ToolbarView

@synthesize delegate = delegate_;
@synthesize animatingTransition = animatingTransition_;
@synthesize hitTestBoundsContraintRelaxed = hitTestBoundsContraintRelaxed_;

// Some views added to the toolbar have bounds larger than the toolbar bounds
// and still needs to receive touches. The overscroll actions view is one of
// those. That method is overridden in order to still perform hit testing on
// subviews that resides outside the toolbar bounds.
- (UIView*)hitTest:(CGPoint)point withEvent:(UIEvent*)event {
  UIView* hitView = [super hitTest:point withEvent:event];
  if (hitView || !self.hitTestBoundsContraintRelaxed)
    return hitView;

  for (UIView* view in [[self subviews] reverseObjectEnumerator]) {
    if (!view.userInteractionEnabled || [view isHidden] || [view alpha] < 0.01)
      continue;
    const CGPoint convertedPoint = [view convertPoint:point fromView:self];
    if ([view pointInside:convertedPoint withEvent:event]) {
      hitView = [view hitTest:convertedPoint withEvent:event];
      if (hitView)
        break;
    }
  }
  return hitView;
}

- (void)setFrame:(CGRect)frame {
  CGRect oldFrame = self.frame;
  [super setFrame:frame];
  [delegate_ frameDidChangeFrame:frame fromFrame:oldFrame];
}

- (void)didMoveToWindow {
  [super didMoveToWindow];
  [delegate_ windowDidChange];
}

- (void)willMoveToSuperview:(UIView*)newSuperview {
  [super willMoveToSuperview:newSuperview];
  if (base::FeatureList::IsEnabled(kSafeAreaCompatibleToolbar)) {
    [self removeConstraints:self.constraints];
  }
}

- (id<CAAction>)actionForLayer:(CALayer*)layer forKey:(NSString*)event {
  // Don't allow UIView block-based animations if we're already performing
  // explicit transition animations.
  if (self.animatingTransition)
    return (id<CAAction>)[NSNull null];
  return [super actionForLayer:layer forKey:event];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  [delegate_ traitCollectionDidChange];
}

@end
