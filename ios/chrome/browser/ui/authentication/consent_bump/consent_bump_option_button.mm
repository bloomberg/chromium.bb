// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_option_button.h"

#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kVerticalMargin = 8;
const CGFloat kTitleTextMargin = 4;

const CGFloat kImageViewMargin = 16;
}  // namespace

@interface ConsentBumpOptionButton ()

// Image view containing the check mark is the option is selected.
@property(nonatomic, strong) UIImageView* checkMarkImageView;

@end

@implementation ConsentBumpOptionButton

@synthesize checked = _checked;
@synthesize type = _type;
@synthesize checkMarkImageView = _checkMarkImageView;

+ (instancetype)consentBumpOptionButtonWithTitle:(NSString*)title
                                            text:(NSString*)text {
  ConsentBumpOptionButton* option = [self buttonWithType:UIButtonTypeCustom];

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.numberOfLines = 0;
  titleLabel.text = title;
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  titleLabel.textColor =
      [UIColor colorWithWhite:0 alpha:kAuthenticationTitleColorAlpha];
  [option addSubview:titleLabel];

  UIView* separator = [[UIView alloc] init];
  separator.backgroundColor =
      [UIColor colorWithWhite:0 alpha:kAuthenticationSeparatorColorAlpha];
  separator.translatesAutoresizingMaskIntoConstraints = NO;
  [option addSubview:separator];

  UIImageView* checkMarkImageView = [[UIImageView alloc] init];
  option.checkMarkImageView = checkMarkImageView;
  checkMarkImageView.translatesAutoresizingMaskIntoConstraints = NO;
  [option addSubview:checkMarkImageView];

  id<LayoutGuideProvider> safeArea = SafeAreaLayoutGuideForView(option);

  if (text) {
    // There is text to be displayed. Make sure it is taken into account.
    UILabel* textLabel = [[UILabel alloc] init];
    textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    textLabel.numberOfLines = 0;
    textLabel.text = text;
    textLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    textLabel.textColor =
        [UIColor colorWithWhite:0 alpha:kAuthenticationTextColorAlpha];
    [option addSubview:textLabel];

    [NSLayoutConstraint activateConstraints:@[
      [textLabel.topAnchor constraintEqualToAnchor:titleLabel.bottomAnchor
                                          constant:kTitleTextMargin],
      [textLabel.leadingAnchor
          constraintEqualToAnchor:safeArea.leadingAnchor
                         constant:kAuthenticationHorizontalMargin],
      [textLabel.bottomAnchor constraintEqualToAnchor:option.bottomAnchor
                                             constant:-kVerticalMargin],
      [textLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:checkMarkImageView.leadingAnchor
                                   constant:-kImageViewMargin],

    ]];
  } else {
    // No text, constraint the title label to the bottom of the option.
    [titleLabel.bottomAnchor constraintEqualToAnchor:option.bottomAnchor
                                            constant:-kVerticalMargin]
        .active = YES;
  }

  AddSameCenterYConstraint(option, checkMarkImageView);

  [NSLayoutConstraint activateConstraints:@[
    // Title.
    [titleLabel.topAnchor constraintEqualToAnchor:option.topAnchor
                                         constant:kVerticalMargin],
    [titleLabel.leadingAnchor
        constraintEqualToAnchor:safeArea.leadingAnchor
                       constant:kAuthenticationHorizontalMargin],

    // Check image.
    [titleLabel.trailingAnchor
        constraintLessThanOrEqualToAnchor:checkMarkImageView.leadingAnchor
                                 constant:-kImageViewMargin],
    [checkMarkImageView.trailingAnchor
        constraintEqualToAnchor:safeArea.trailingAnchor
                       constant:-kImageViewMargin],
    [checkMarkImageView.topAnchor
        constraintGreaterThanOrEqualToAnchor:option.topAnchor
                                    constant:kImageViewMargin],
    [checkMarkImageView.bottomAnchor
        constraintLessThanOrEqualToAnchor:option.bottomAnchor
                                 constant:-kImageViewMargin],

    // Separator.
    [separator.heightAnchor
        constraintEqualToConstant:kAuthenticationSeparatorHeight],
    [separator.leadingAnchor
        constraintEqualToAnchor:safeArea.leadingAnchor
                       constant:kAuthenticationHorizontalMargin],
    [separator.trailingAnchor constraintEqualToAnchor:option.trailingAnchor],
    [separator.bottomAnchor constraintEqualToAnchor:option.bottomAnchor],
  ]];

  return option;
}

- (void)setChecked:(BOOL)checked {
  if (checked == _checked)
    return;

  _checked = checked;
  // TODO(crbug.com/866506): Update image.
}

@end
