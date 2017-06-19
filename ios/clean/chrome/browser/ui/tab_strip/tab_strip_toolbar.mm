// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab_strip/tab_strip_toolbar.h"

#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/clean/chrome/browser/ui/actions/tab_strip_actions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
CGFloat kSpacing = 10.0f;
}

@implementation TabStripToolbar

+ (NSString*)reuseIdentifier {
  return NSStringFromClass([self class]);
}

- (instancetype)initWithFrame:(CGRect)frame {
  if ((self = [super initWithFrame:frame])) {
    UIView* toolbar = [self toolbar];
    toolbar.frame = self.bounds;
    toolbar.translatesAutoresizingMaskIntoConstraints = YES;
    toolbar.autoresizingMask =
        UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [self addSubview:toolbar];
  }
  return self;
}

#pragma mark - Private

- (UIView*)toolbar {
  NSArray* items = @[ [self closeButton], [self incognitoButton] ];
  UIStackView* toolbar = [[UIStackView alloc] initWithArrangedSubviews:items];
  toolbar.axis = UILayoutConstraintAxisVertical;
  toolbar.layoutMarginsRelativeArrangement = YES;
  toolbar.layoutMargins = UIEdgeInsetsMake(0, 0, kSpacing, 0);
  toolbar.distribution = UIStackViewDistributionFillEqually;
  return toolbar;
}

- (UIButton*)closeButton {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  [button setImage:[ChromeIcon closeIcon] forState:UIControlStateNormal];
  [button setTintColor:[UIColor whiteColor]];
  // TODO(crbug.com/733453): Use dispatcher instead of responder chain.
  [button addTarget:nil
                action:@selector(hideTabStrip:)
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

- (UIButton*)incognitoButton {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  UIImage* image = [UIImage imageNamed:@"tabswitcher_incognito"];
  [button setImage:image forState:UIControlStateNormal];
  [button setTintColor:[UIColor whiteColor]];
  return button;
}

@end
