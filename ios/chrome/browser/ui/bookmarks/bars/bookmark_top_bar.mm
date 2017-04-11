// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/ui/bookmarks/bars/bookmark_top_bar.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BookmarkTopBar

@synthesize contentView = _contentView;

+ (CGFloat)expectedContentViewHeight {
  return 56.0;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.autoresizingMask = UIViewAutoresizingFlexibleBottomMargin |
                            UIViewAutoresizingFlexibleWidth;

    _contentView = [[UIView alloc] init];
    [self addSubview:_contentView];

    [self statelessLayoutContentView];
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];

  // The content view.
  [self statelessLayoutContentView];
}

- (void)statelessLayoutContentView {
  self.contentView.frame = CGRectMake(
      0,
      CGRectGetHeight(self.bounds) - [[self class] expectedContentViewHeight],
      CGRectGetWidth(self.bounds), [[self class] expectedContentViewHeight]);
}

@end
