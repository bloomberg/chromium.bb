// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/search_widget_extension/search_action_view.h"

#import <NotificationCenter/NotificationCenter.h>

#include "base/ios/ios_util.h"
#import "ios/chrome/search_widget_extension/ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kActionButtonSize = 55;
const CGFloat kIconSize = 35;

}  // namespace

@implementation SearchActionView

- (instancetype)initWithActionTarget:(id)target
                      actionSelector:(SEL)actionSelector
                               title:(NSString*)title
                           imageName:(NSString*)imageName {
  DCHECK(target);
  self = [super initWithFrame:CGRectZero];
  if (self) {
    self.translatesAutoresizingMaskIntoConstraints = NO;

    UIVibrancyEffect* primaryEffect;
    UIVibrancyEffect* secondaryEffect;
    if (@available(iOS 10, *)) {
      primaryEffect = [UIVibrancyEffect widgetPrimaryVibrancyEffect];
      secondaryEffect = [UIVibrancyEffect widgetSecondaryVibrancyEffect];
    }
#if !defined(__IPHONE_10_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_10_0
    else {
      primaryEffect = [UIVibrancyEffect notificationCenterVibrancyEffect];
      secondaryEffect = [UIVibrancyEffect notificationCenterVibrancyEffect];
    }
#endif  // !defined(__IPHONE_10_0) || __IPHONE_OS_VERSION_MIN_REQUIRED <
        // __IPHONE_10_0

    UIVisualEffectView* primaryEffectView =
        [[UIVisualEffectView alloc] initWithEffect:primaryEffect];
    UIVisualEffectView* secondaryEffectView =
        [[UIVisualEffectView alloc] initWithEffect:secondaryEffect];
    for (UIVisualEffectView* effectView in
         @[ primaryEffectView, secondaryEffectView ]) {
      [self addSubview:effectView];
      effectView.translatesAutoresizingMaskIntoConstraints = NO;
      effectView.userInteractionEnabled = NO;
      [NSLayoutConstraint
          activateConstraints:ui_util::CreateSameConstraints(self, effectView)];
    }

    UIView* circleView = [[UIView alloc] initWithFrame:CGRectZero];
    if (@available(iOS 10, *)) {
      circleView.backgroundColor = [UIColor colorWithWhite:0 alpha:0.05];
    } else {
      circleView.backgroundColor = [UIColor whiteColor];
    }
    circleView.layer.cornerRadius = kActionButtonSize / 2;

    UILabel* labelView = [[UILabel alloc] initWithFrame:CGRectZero];
    labelView.text = title;
    labelView.numberOfLines = 0;
    labelView.textAlignment = NSTextAlignmentCenter;
    labelView.font = [UIFont preferredFontForTextStyle:UIFontTextStyleCaption2];
    labelView.isAccessibilityElement = NO;
    [labelView
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:UILayoutConstraintAxisVertical];

    UIStackView* stack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ circleView, labelView ]];
    stack.axis = UILayoutConstraintAxisVertical;
    stack.spacing = ui_util::kIconSpacing;
    stack.alignment = UIStackViewAlignmentCenter;
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    [secondaryEffectView.contentView addSubview:stack];
    [NSLayoutConstraint activateConstraints:ui_util::CreateSameConstraints(
                                                secondaryEffectView, stack)];
    UIImage* iconImage = [UIImage imageNamed:imageName];
    if (@available(iOS 10, *)) {
      iconImage =
          [iconImage imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    }

    UIImageView* icon = [[UIImageView alloc] initWithImage:iconImage];
    icon.translatesAutoresizingMaskIntoConstraints = NO;
    if (@available(iOS 10, *)) {
      [primaryEffectView.contentView addSubview:icon];
    } else {
      [self addSubview:icon];
    }

    [NSLayoutConstraint activateConstraints:@[
      [circleView.widthAnchor constraintEqualToConstant:kActionButtonSize],
      [circleView.heightAnchor constraintEqualToConstant:kActionButtonSize],
      [icon.widthAnchor constraintEqualToConstant:kIconSize],
      [icon.heightAnchor constraintEqualToConstant:kIconSize],
      [icon.centerXAnchor constraintEqualToAnchor:circleView.centerXAnchor],
      [icon.centerYAnchor constraintEqualToAnchor:circleView.centerYAnchor],
    ]];

    self.userInteractionEnabled = YES;
    [self addTarget:target
                  action:actionSelector
        forControlEvents:UIControlEventTouchUpInside];
    self.accessibilityLabel = title;
  }
  return self;
}

@end
