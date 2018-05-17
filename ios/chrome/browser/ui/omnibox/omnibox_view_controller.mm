// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_view_controller.h"

#import "ios/chrome/browser/ui/location_bar/omnibox_container_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OmniboxViewController ()

// Override of UIViewController's view with a different type.
@property(nonatomic, strong) OmniboxContainerView* view;

@property(nonatomic, strong) UIFont* textFieldFont;
@property(nonatomic, strong) UIColor* textFieldTintColor;
@property(nonatomic, strong) UIColor* textFieldTextColor;

@end

@implementation OmniboxViewController
@synthesize textFieldFont = _textFieldFont;
@synthesize textFieldTintColor = _textFieldTintColor;
@synthesize textFieldTextColor = _textFieldTextColor;
@dynamic view;

- (instancetype)initWithFont:(UIFont*)font
                   textColor:(UIColor*)textColor
                   tintColor:(UIColor*)tintColor {
  self = [super init];
  if (self) {
    _textFieldFont = font;
    _textFieldTextColor = textColor;
    _textFieldTintColor = tintColor;
  }
  return self;
}

#pragma mark - UIViewController

- (void)loadView {
  self.view =
      [[OmniboxContainerView alloc] initWithFrame:CGRectZero
                                             font:self.textFieldFont
                                        textColor:self.textFieldTextColor
                                        tintColor:self.textFieldTintColor];
}

#pragma mark - public methods

- (OmniboxTextFieldIOS*)textField {
  return self.view.textField;
}

@end
