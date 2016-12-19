// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/contextual_search/contextual_search_mask_view.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/contextual_search/contextual_search_panel_view.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"

// Linearly map |delta| in the range [0, 1] to a value in [min, max].
#define LERP(min, max, delta) (min * (1 - delta) + max * delta)

namespace {
const CGFloat kPhoneMaskLimit = 1.0;
const CGFloat kPadMaskLimit = 0.8;
}

@implementation ContextualSearchMaskView {
  CGFloat _maskLimit;
}

- (instancetype)init {
  if ((self = [super initWithFrame:CGRectZero])) {
    self.userInteractionEnabled = NO;
    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.accessibilityIdentifier = @"contextualSearchMask";
    self.alpha = 0.0;
    self.backgroundColor = [UIColor blackColor];
    _maskLimit = IsIPadIdiom() ? kPadMaskLimit : kPhoneMaskLimit;
  }
  return self;
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE {
  NOTREACHED();
  return nil;
}

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE {
  NOTREACHED();
  return nil;
}

#pragma mark - UIView methods.

- (void)updateConstraints {
  DCHECK(self.superview);
  NSArray* constraints = @[ @"V:|[mask]|", @"H:|[mask]|" ];
  ApplyVisualConstraints(constraints, @{ @"mask" : self }, self.superview);
  [super updateConstraints];
}

#pragma mark - ContextualSearchPanelMotionObserver methods

- (void)panel:(ContextualSearchPanelView*)panel
    didMoveWithMotion:(ContextualSearch::PanelMotion)motion {
  CGFloat ratio;
  if (motion.state < ContextualSearch::PEEKING) {
    ratio = 0;
  } else if (motion.state == ContextualSearch::COVERING) {
    ratio = 1;
  } else {
    ratio = [panel.configuration gradationToState:ContextualSearch::COVERING
                                        fromState:ContextualSearch::PEEKING
                                       atPosition:motion.position];
  }

  ratio = LERP(0, _maskLimit, ratio);

  self.alpha = ratio * ratio;
}

- (void)panelWillPromote:(ContextualSearchPanelView*)panel {
  [panel removeMotionObserver:self];
}

@end
