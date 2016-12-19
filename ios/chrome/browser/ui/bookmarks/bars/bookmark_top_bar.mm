// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "ios/chrome/browser/ui/bookmarks/bars/bookmark_top_bar.h"

#include "base/mac/objc_property_releaser.h"
#include "base/mac/scoped_nsobject.h"

@interface BookmarkTopBar () {
  base::mac::ObjCPropertyReleaser _propertyReleaser_BookmarkBar;
}
@property(nonatomic, retain) UIView* contentView;
@end

@implementation BookmarkTopBar

@synthesize contentView = _contentView;

+ (CGFloat)expectedContentViewHeight {
  return 56.0;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _propertyReleaser_BookmarkBar.Init(self, [BookmarkTopBar class]);

    self.autoresizingMask = UIViewAutoresizingFlexibleBottomMargin |
                            UIViewAutoresizingFlexibleWidth;

    base::scoped_nsobject<UIView> contentView([[UIView alloc] init]);
    self.contentView.backgroundColor = [UIColor clearColor];
    [self addSubview:contentView];
    self.contentView = contentView;

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
