// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/table_view_empty_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The StackView vertical spacing.
const float kStackViewVerticalSpacing = 23.0;
// The StackView width.
const float kStackViewWidth = 227.0;
}

@interface TableViewEmptyView ()
// The message that will be displayed.
@property(nonatomic, copy) NSString* message;
// The image that will be displayed.
@property(nonatomic, strong) UIImage* image;
@end

@implementation TableViewEmptyView
@synthesize message = _message;
@synthesize image = _image;

- (instancetype)initWithFrame:(CGRect)frame
                      message:(NSString*)message
                        image:(UIImage*)image {
  self = [super initWithFrame:frame];
  if (self) {
    self.message = message;
    self.image = image;
  }
  return self;
}

- (void)willMoveToSuperview:(UIView*)newSuperview {
  // Return if the subviews have already been created and added.
  if (!(self.subviews.count == 0))
    return;

  UIImageView* imageView = [[UIImageView alloc] initWithImage:self.image];
  imageView.contentMode = UIViewContentModeScaleAspectFit;
  imageView.clipsToBounds = YES;

  UILabel* messageLabel = [[UILabel alloc] init];
  messageLabel.text = self.message;
  messageLabel.numberOfLines = 0;
  messageLabel.lineBreakMode = NSLineBreakByWordWrapping;
  messageLabel.textAlignment = NSTextAlignmentCenter;
  messageLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  messageLabel.textColor = [UIColor grayColor];

  // Vertical stack view that holds the image and message.
  UIStackView* verticalStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ imageView, messageLabel ]];
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.spacing = kStackViewVerticalSpacing;
  verticalStack.distribution = UIStackViewDistributionFill;
  verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
  [self addSubview:verticalStack];

  [NSLayoutConstraint activateConstraints:@[
    [verticalStack.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    [verticalStack.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
    [verticalStack.widthAnchor constraintEqualToConstant:kStackViewWidth]
  ]];
}

@end
