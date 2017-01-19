// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/widget_extension/widget_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kCursorHeight = 40;
const CGFloat kCursorWidth = 2;
const CGFloat kCursorHorizontalPadding = 40;

}  // namespace

@interface WidgetView ()

@property(nonatomic, weak) UIView* cursor;

// Creates a cursor, adds it to the view and sets the class cursor property.
- (void)addCursor;

@end

@implementation WidgetView

@synthesize cursor = _cursor;

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    [self addCursor];
  }
  return self;
}

- (void)addCursor {
  UIView* cursor = [[UIView alloc] initWithFrame:CGRectZero];
  self.cursor = cursor;
  self.cursor.backgroundColor = [UIColor blueColor];
  [self addSubview:self.cursor];

  [self.cursor setTranslatesAutoresizingMaskIntoConstraints:NO];
  [NSLayoutConstraint activateConstraints:@[
    [[self.cursor widthAnchor] constraintEqualToConstant:kCursorWidth],
    [[self.cursor leadingAnchor]
        constraintEqualToAnchor:self.leadingAnchor
                       constant:kCursorHorizontalPadding],
    [[self.cursor heightAnchor] constraintEqualToConstant:kCursorHeight],
    [[self.cursor centerYAnchor] constraintEqualToAnchor:self.centerYAnchor]
  ]];

  [UIView animateWithDuration:0.3
                        delay:0.0
                      options:UIViewAnimationOptionRepeat |
                              UIViewAnimationOptionAutoreverse
                   animations:^{
                     self.cursor.alpha = 0.0f;
                   }
                   completion:nil];
}

@end
