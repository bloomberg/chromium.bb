// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bubble/bubble_view.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BubbleView ()
// Label containing the text displayed on the bubble.
@property(nonatomic, weak) UILabel* label;
@end

@implementation BubbleView

@synthesize label = _label;

- (instancetype)initWithText:(NSString*)text
                   direction:(BubbleArrowDirection)arrowDirection
                   alignment:(BubbleAlignment)alignment {
  self = [super initWithFrame:CGRectZero];
  return self;
}

#pragma mark - UIView overrides

// Override sizeThatFits to return the bubble's optimal size.
- (CGSize)sizeThatFits:(CGSize)size {
  NOTIMPLEMENTED();
  return size;
}

@end
