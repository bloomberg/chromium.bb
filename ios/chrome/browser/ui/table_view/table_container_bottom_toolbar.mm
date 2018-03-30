// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/table_container_bottom_toolbar.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Horizontal margins.
const CGFloat kHorizontalMargin = 10.0;
}

@interface TableContainerBottomToolbar ()
// Re-declare as readwrite.
@property(nonatomic, strong, readwrite) UIButton* leadingButton;
// Re-declare as readwrite.
@property(nonatomic, strong, readwrite) UIButton* trailingButton;
// StackView that contains the buttons.
@property(nonatomic, strong) UIStackView* stackView;
@end

@implementation TableContainerBottomToolbar
@synthesize leadingButton = _leadingButton;
@synthesize trailingButton = _trailingButton;
@synthesize stackView = _stackView;

- (instancetype)initWithLeadingButtonText:(NSString*)leadingButtonText
                       trailingButtonText:(NSString*)trailingButtonText {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _stackView = [[UIStackView alloc] initWithFrame:CGRectZero];
    // Configure and add the buttons to _stackView.
    if (leadingButtonText && ![leadingButtonText isEqualToString:@""]) {
      _leadingButton = [UIButton buttonWithType:UIButtonTypeCustom];
      [_leadingButton setTitle:leadingButtonText forState:UIControlStateNormal];
      [_leadingButton setTitleColor:self.tintColor
                           forState:UIControlStateNormal];
      [_stackView addArrangedSubview:_leadingButton];
    }

    if (trailingButtonText && ![trailingButtonText isEqualToString:@""]) {
      _trailingButton = [UIButton buttonWithType:UIButtonTypeCustom];
      [_trailingButton setTitle:trailingButtonText
                       forState:UIControlStateNormal];
      [_trailingButton setTitleColor:self.tintColor
                            forState:UIControlStateNormal];
      [_stackView addArrangedSubview:_trailingButton];
    }

    // Configure the stackView.
    _stackView.translatesAutoresizingMaskIntoConstraints = NO;
    _stackView.distribution = UIStackViewDistributionEqualSpacing;
    _stackView.axis = UILayoutConstraintAxisHorizontal;
    _stackView.alignment = UIStackViewAlignmentCenter;
    [self addSubview:_stackView];

    // Set and activate constraints.
    [NSLayoutConstraint activateConstraints:@[
      [_stackView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor
                                               constant:kHorizontalMargin],
      [_stackView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor
                                                constant:-kHorizontalMargin],
      [_stackView.topAnchor constraintEqualToAnchor:self.topAnchor],
      [_stackView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    ]];
  }
  return self;
}

@end
