// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"

#import "ios/chrome/browser/ui/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The width and height of the favicon ImageView.
const int kTableViewBackgroundRGBColor = 0xF9F9F9;
// The default color used to highlight tableview cells.
const CGFloat kTableViewHighlightColorAlpha = 0.05;
}

@implementation ChromeTableViewStyler
@synthesize tableViewSectionHeaderBlurEffect =
    _tableViewSectionHeaderBlurEffect;
@synthesize tableViewBackgroundColor = _tableViewBackgroundColor;
@synthesize cellTitleColor = _cellTitleColor;
@synthesize headerFooterTitleColor = _headerFooterTitleColor;
@synthesize cellHighlightColor = _cellHighlightColor;

- (instancetype)init {
  if ((self = [super init])) {
    _tableViewBackgroundColor = UIColorFromRGB(kTableViewBackgroundRGBColor, 1);

    _tableViewSectionHeaderBlurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleExtraLight];

    _cellHighlightColor =
        [UIColor colorWithWhite:0 alpha:kTableViewHighlightColorAlpha];
  }
  return self;
}

@end
