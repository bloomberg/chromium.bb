// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/clean/toolbar_configuration.h"

#import "ios/chrome/browser/ui/toolbar/clean/toolbar_constants.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ToolbarConfiguration

@synthesize style = _style;

- (instancetype)initWithStyle:(ToolbarStyle)style {
  self = [super init];
  if (self) {
    _style = style;
  }
  return self;
}

- (UIColor*)backgroundColor {
  switch (self.style) {
    case NORMAL:
      return UIColorFromRGB(kToolbarBackgroundColor);
    case INCOGNITO:
      return UIColorFromRGB(kIncognitoToolbarBackgroundColor);
  }
}

- (UIColor*)omniboxBackgroundColor {
  switch (self.style) {
    case NORMAL:
      return [UIColor whiteColor];
    case INCOGNITO:
      return UIColorFromRGB(kIcongnitoLocationBackgroundColor);
  }
}

- (UIColor*)omniboxBorderColor {
  switch (self.style) {
    case NORMAL:
      return UIColorFromRGB(kLocationBarBorderColor);
    case INCOGNITO:
      return UIColorFromRGB(kIncognitoLocationBarBorderColor);
  }
}

- (UIColor*)buttonTitleNormalColor {
  switch (self.style) {
    case NORMAL:
      return UIColorFromRGB(kToolbarButtonTitleNormalColor);
    case INCOGNITO:
      return UIColorFromRGB(kIncognitoToolbarButtonTitleNormalColor);
  }
}

- (UIColor*)buttonTitleHighlightedColor {
  switch (self.style) {
    case NORMAL:
      return UIColorFromRGB(kToolbarButtonTitleHighlightedColor);
    case INCOGNITO:
      return UIColorFromRGB(kIncognitoToolbarButtonTitleHighlightedColor);
  }
}

@end
