// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/keyboard_accessory_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ManualFillKeyboardAccessoryView ()

@property(nonatomic, readonly, weak) id<ManualFillKeyboardAccessoryViewDelegate>
    delegate;

@end

@implementation ManualFillKeyboardAccessoryView

@synthesize delegate = _delegate;

- (instancetype)initWithDelegate:
    (id<ManualFillKeyboardAccessoryViewDelegate>)delegate {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _delegate = delegate;
    UIColor* tintColor = [UIColor colorWithRed:115.0 / 255.0
                                         green:115.0 / 255.0
                                          blue:115.0 / 255.0
                                         alpha:1.0];

    UIButton* passwordButton = [UIButton buttonWithType:UIButtonTypeCustom];
    UIImage* keyImage = [UIImage imageNamed:@"ic_vpn_key"];
    [passwordButton setImage:keyImage forState:UIControlStateNormal];
    passwordButton.tintColor = tintColor;
    passwordButton.translatesAutoresizingMaskIntoConstraints = NO;
    [passwordButton addTarget:_delegate
                       action:@selector(passwordButtonPressed)
             forControlEvents:UIControlEventTouchUpInside];
    [self addSubview:passwordButton];

    UIButton* cardsButton = [UIButton buttonWithType:UIButtonTypeCustom];
    UIImage* cardImage = [UIImage imageNamed:@"ic_credit_card"];
    [cardsButton setImage:cardImage forState:UIControlStateNormal];
    cardsButton.tintColor = tintColor;
    cardsButton.translatesAutoresizingMaskIntoConstraints = NO;
    [cardsButton addTarget:_delegate
                    action:@selector(cardButtonPressed)
          forControlEvents:UIControlEventTouchUpInside];
    [self addSubview:cardsButton];

    UIButton* accountButton = [UIButton buttonWithType:UIButtonTypeCustom];
    UIImage* accountImage = [UIImage imageNamed:@"ic_account_circle"];
    [accountButton setImage:accountImage forState:UIControlStateNormal];
    accountButton.tintColor = tintColor;
    accountButton.translatesAutoresizingMaskIntoConstraints = NO;
    [accountButton addTarget:_delegate
                      action:@selector(accountButtonPressed)
            forControlEvents:UIControlEventTouchUpInside];
    [self addSubview:accountButton];

    NSLayoutXAxisAnchor* menuLeadingAnchor = self.leadingAnchor;
    if (@available(iOS 11, *)) {
      menuLeadingAnchor = self.safeAreaLayoutGuide.leadingAnchor;
    }

    [NSLayoutConstraint activateConstraints:@[
      // Vertical constraints.
      [passwordButton.heightAnchor constraintEqualToAnchor:self.heightAnchor],
      [passwordButton.topAnchor constraintEqualToAnchor:self.topAnchor],

      [cardsButton.heightAnchor constraintEqualToAnchor:self.heightAnchor],
      [cardsButton.topAnchor constraintEqualToAnchor:self.topAnchor],

      [accountButton.heightAnchor constraintEqualToAnchor:self.heightAnchor],
      [accountButton.topAnchor constraintEqualToAnchor:self.topAnchor],

      // Horizontal constraints.
      [passwordButton.leadingAnchor constraintEqualToAnchor:menuLeadingAnchor
                                                   constant:12],

      [cardsButton.leadingAnchor
          constraintEqualToAnchor:passwordButton.trailingAnchor
                         constant:8],

      [accountButton.leadingAnchor
          constraintEqualToAnchor:cardsButton.trailingAnchor
                         constant:8],
    ]];
  }

  return self;
}

@end
