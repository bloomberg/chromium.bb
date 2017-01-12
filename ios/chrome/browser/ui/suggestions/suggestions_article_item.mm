// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/suggestions/suggestions_article_item.h"

#import "ios/chrome/browser/ui/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kImageSize = 100;
}

@interface SuggestionsArticleItem ()

@property(nonatomic, copy) NSString* title;
@property(nonatomic, copy) NSString* subtitle;
@property(nonatomic, strong) UIImage* image;

@end

@implementation SuggestionsArticleItem

@synthesize title = _title;
@synthesize subtitle = _subtitle;
@synthesize image = _image;

- (instancetype)initWithType:(NSInteger)type
                       title:(NSString*)title
                    subtitle:(NSString*)subtitle
                       image:(UIImage*)image {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [SuggestionsArticleCell class];
    _title = [title copy];
    _subtitle = [subtitle copy];
    _image = image;
  }
  return self;
}

- (void)configureCell:(SuggestionsArticleCell*)cell {
  [super configureCell:cell];
  cell.titleLabel.text = _title;
  cell.subtitleLabel.text = _subtitle;
  cell.imageView.image = _image;
}

@end

@implementation SuggestionsArticleCell

@synthesize titleLabel = _titleLabel;
@synthesize subtitleLabel = _subtitleLabel;
@synthesize imageView = _imageView;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _titleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _subtitleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _imageView = [[UIImageView alloc] initWithFrame:CGRectZero];
    UIView* imageContainer = [[UIView alloc] initWithFrame:CGRectZero];

    _subtitleLabel.numberOfLines = 0;

    imageContainer.translatesAutoresizingMaskIntoConstraints = NO;
    _imageView.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;

    [imageContainer addSubview:_imageView];
    [self.contentView addSubview:imageContainer];
    [self.contentView addSubview:_titleLabel];
    [self.contentView addSubview:_subtitleLabel];

    ApplyVisualConstraintsWithMetrics(
        @[
          @"H:|-[title]-[imageContainer(imageSize)]-|",
          @"H:|[image(imageSize)]", @"H:|-[text]-[imageContainer]",
          @"V:|[image(imageSize)]", @"V:|-[title]-[text]-|",
          @"V:|-[imageContainer(>=imageSize)]-|"
        ],
        @{
          @"image" : _imageView,
          @"imageContainer" : imageContainer,
          @"title" : _titleLabel,
          @"text" : _subtitleLabel
        },
        @{ @"imageSize" : @(kImageSize) });
  }
  return self;
}

#pragma mark - UIView

// Implements -layoutSubviews as per instructions in documentation for
// +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
- (void)layoutSubviews {
  [super layoutSubviews];

  // Adjust the text label preferredMaxLayoutWidth when the parent's width
  // changes, for instance on screen rotation.
  CGFloat parentWidth = CGRectGetWidth(self.contentView.bounds);
  self.subtitleLabel.preferredMaxLayoutWidth = parentWidth - kImageSize - 3 * 8;

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

@end
