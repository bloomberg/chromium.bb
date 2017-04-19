// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/search_widget_extension/search_widget_view.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kCursorHeight = 40;
const CGFloat kCursorWidth = 2;
const CGFloat kCursorHorizontalPadding = 10;
const CGFloat kCursorVerticalPadding = 10;
const CGFloat kFakeboxHorizontalPadding = 40;
const CGFloat kFakeboxVerticalPadding = 40;

}  // namespace

@interface SearchWidgetView () {
  __weak id<SearchWidgetViewActionTarget> _target;
}

@property(nonatomic, copy) NSString* copiedURL;
@property(nonatomic, strong) UILabel* copiedURLLabel;
@property(nonatomic, weak) UIView* cursor;

// Creates and adds a fake omnibox with blinking cursor to the view and sets the
// class cursor property.
- (void)addFakebox;

@end

@implementation SearchWidgetView

@synthesize copiedURL = _copiedURL;
@synthesize copiedURLLabel = _copiedURLLabel;

@synthesize cursor = _cursor;

- (instancetype)initWithActionTarget:(id<SearchWidgetViewActionTarget>)target {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    DCHECK(target);
    _target = target;
    [self addFakebox];
  }
  return self;
}

- (void)addFakebox {
  UIView* fakebox = [[UIView alloc] initWithFrame:CGRectZero];

  UIGestureRecognizer* tapRecognizer =
      [[UITapGestureRecognizer alloc] initWithTarget:_target
                                              action:@selector(openSearch:)];

  [fakebox addGestureRecognizer:tapRecognizer];
  [self addSubview:fakebox];

  UIView* cursor = [[UIView alloc] initWithFrame:CGRectZero];
  self.cursor = cursor;
  self.cursor.backgroundColor = [UIColor blueColor];
  [fakebox addSubview:self.cursor];

  [fakebox setTranslatesAutoresizingMaskIntoConstraints:NO];
  [self.cursor setTranslatesAutoresizingMaskIntoConstraints:NO];
  [NSLayoutConstraint activateConstraints:@[
    [[fakebox leadingAnchor] constraintEqualToAnchor:self.leadingAnchor
                                            constant:kFakeboxHorizontalPadding],
    [[fakebox trailingAnchor]
        constraintEqualToAnchor:self.trailingAnchor
                       constant:-kFakeboxHorizontalPadding],
    [[fakebox topAnchor] constraintEqualToAnchor:self.topAnchor
                                        constant:kFakeboxVerticalPadding],
    [[fakebox heightAnchor]
        constraintEqualToConstant:kCursorHeight + 2 * kCursorVerticalPadding],

    [[self.cursor widthAnchor] constraintEqualToConstant:kCursorWidth],
    [[self.cursor leadingAnchor]
        constraintEqualToAnchor:fakebox.leadingAnchor
                       constant:kCursorHorizontalPadding],
    [[self.cursor heightAnchor] constraintEqualToConstant:kCursorHeight],
    [[self.cursor centerYAnchor] constraintEqualToAnchor:fakebox.centerYAnchor]
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

- (void)updateCopiedURL:(NSString*)copiedURL {
  self.copiedURL = copiedURL;
  self.copiedURLLabel.text = copiedURL;
}

@end
