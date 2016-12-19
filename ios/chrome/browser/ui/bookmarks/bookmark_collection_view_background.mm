// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/bookmarks/bookmark_collection_view_background.h"

#include "base/mac/objc_property_releaser.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"

namespace {
NSString* const kBookmarkGrayStar = @"bookmark_gray_star_large";
const CGFloat kEmptyBookmarkTextSize = 16.0;
// Offset of the image view on top of the text.
const CGFloat kImageViewOffsetFromText = 5.0;
}  // namespace

@interface BookmarkCollectionViewBackground () {
  base::mac::ObjCPropertyReleaser
      _propertyReleaser_BookmarkBookmarkCollectionViewBackground;
}

// Star image view shown on top of the label.
@property(nonatomic, retain) UIImageView* emptyBookmarksImageView;
// Label centered on the view showing the empty bookmarks text.
@property(nonatomic, retain) UILabel* emptyBookmarksLabel;

@end

@implementation BookmarkCollectionViewBackground

@synthesize emptyBookmarksImageView = _emptyBookmarksImageView;
@synthesize emptyBookmarksLabel = _emptyBookmarksLabel;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _propertyReleaser_BookmarkBookmarkCollectionViewBackground.Init(
        self, [BookmarkCollectionViewBackground class]);
    _emptyBookmarksImageView = [self newBookmarkImageView];
    [self addSubview:_emptyBookmarksImageView];
    _emptyBookmarksLabel = [self newEmptyBookmarkLabel];
    [self addSubview:_emptyBookmarksLabel];
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  _emptyBookmarksLabel.frame = [self emptyBookmarkLabelFrame];
  _emptyBookmarksImageView.frame = [self bookmarkImageViewFrame];
}

- (NSString*)text {
  return self.emptyBookmarksLabel.text;
}

- (void)setText:(NSString*)text {
  self.emptyBookmarksLabel.text = text;
  [self setNeedsLayout];
}

#pragma mark - Private

- (UILabel*)newEmptyBookmarkLabel {
  UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
  label.backgroundColor = [UIColor clearColor];
  label.font = [[MDFRobotoFontLoader sharedInstance]
      mediumFontOfSize:kEmptyBookmarkTextSize];
  label.textColor = [UIColor colorWithWhite:0 alpha:110.0 / 255];
  label.textAlignment = NSTextAlignmentCenter;
  return label;
}

- (UIImageView*)newBookmarkImageView {
  UIImageView* imageView = [[UIImageView alloc] initWithFrame:CGRectZero];
  imageView.image = [UIImage imageNamed:kBookmarkGrayStar];
  return imageView;
}

// Returns vertically centered label frame.
- (CGRect)emptyBookmarkLabelFrame {
  const CGSize labelSizeThatFit =
      [self.emptyBookmarksLabel sizeThatFits:CGSizeZero];
  return CGRectMake(
      0, (CGRectGetHeight(self.bounds) - labelSizeThatFit.height) / 2.0,
      CGRectGetWidth(self.bounds), labelSizeThatFit.height);
}

// Returns imageView frame above the text with kImageViewOffsetFromText from
// text.
- (CGRect)bookmarkImageViewFrame {
  const CGRect labelRect = [self emptyBookmarkLabelFrame];
  const CGSize imageViewSize = self.emptyBookmarksImageView.image.size;
  return CGRectMake((CGRectGetWidth(self.bounds) - imageViewSize.width) / 2.0,
                    CGRectGetMinY(labelRect) - kImageViewOffsetFromText -
                        imageViewSize.height,
                    imageViewSize.width, imageViewSize.height);
}

@end
