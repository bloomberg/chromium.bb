// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_article_item.h"

#include "base/time/time.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kImageSize = 100;
const CGFloat kStandardSpacing = 8;
}

@interface ContentSuggestionsArticleItem ()

@property(nonatomic, copy) NSString* subtitle;

@end

#pragma mark - ContentSuggestionsArticleItem

@implementation ContentSuggestionsArticleItem

@synthesize title = _title;
@synthesize subtitle = _subtitle;
@synthesize image = _image;
@synthesize articleURL = _articleURL;
@synthesize publisher = _publisher;
@synthesize publishDate = _publishDate;

- (instancetype)initWithType:(NSInteger)type
                       title:(NSString*)title
                    subtitle:(NSString*)subtitle
                       image:(UIImage*)image
                         url:(const GURL&)url {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsArticleCell class];
    _title = [title copy];
    _subtitle = [subtitle copy];
    _image = image;
    _articleURL = url;
  }
  return self;
}

- (void)configureCell:(ContentSuggestionsArticleCell*)cell {
  [super configureCell:cell];
  cell.titleLabel.text = _title;
  cell.subtitleLabel.text = _subtitle;
  cell.imageView.image = _image;
  [cell setPublisherName:self.publisher date:self.publishDate];
}

@end

#pragma mark - ContentSuggestionsArticleCell

@interface ContentSuggestionsArticleCell ()

@property(nonatomic, strong) UILabel* publisherLabel;

@end

@implementation ContentSuggestionsArticleCell

@synthesize titleLabel = _titleLabel;
@synthesize subtitleLabel = _subtitleLabel;
@synthesize imageView = _imageView;
@synthesize publisherLabel = _publisherLabel;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _titleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _subtitleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _imageView = [[UIImageView alloc] initWithFrame:CGRectZero];
    _publisherLabel = [[UILabel alloc] initWithFrame:CGRectZero];

    _subtitleLabel.numberOfLines = 0;
    [_subtitleLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                      forAxis:UILayoutConstraintAxisVertical];
    [_titleLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                   forAxis:UILayoutConstraintAxisVertical];
    _imageView.contentMode = UIViewContentModeScaleAspectFit;

    _imageView.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _publisherLabel.translatesAutoresizingMaskIntoConstraints = NO;

    [self.contentView addSubview:_imageView];
    [self.contentView addSubview:_titleLabel];
    [self.contentView addSubview:_subtitleLabel];
    [self.contentView addSubview:_publisherLabel];

    [NSLayoutConstraint activateConstraints:@[
      [_imageView.widthAnchor constraintLessThanOrEqualToConstant:kImageSize],
      [_imageView.heightAnchor constraintLessThanOrEqualToConstant:kImageSize],
      [_publisherLabel.topAnchor
          constraintGreaterThanOrEqualToAnchor:_imageView.bottomAnchor
                                      constant:kStandardSpacing],
      [_publisherLabel.topAnchor
          constraintGreaterThanOrEqualToAnchor:_subtitleLabel.bottomAnchor
                                      constant:kStandardSpacing],
    ]];

    ApplyVisualConstraints(
        @[
          @"H:|-[title]-[image]-|",
          @"H:|-[text]-[image]",
          @"V:|-[title]-[text]",
          @"V:|-[image]",
          @"H:|-[publish]-|",
          @"V:[publish]-|",
        ],
        @{
          @"image" : _imageView,
          @"title" : _titleLabel,
          @"text" : _subtitleLabel,
          @"publish" : _publisherLabel,
        });
  }
  return self;
}

- (void)setPublisherName:(NSString*)publisherName date:(base::Time)publishDate {
  NSDate* date = [NSDate dateWithTimeIntervalSince1970:publishDate.ToDoubleT()];
  NSString* dateString =
      [NSDateFormatter localizedStringFromDate:date
                                     dateStyle:NSDateFormatterMediumStyle
                                     timeStyle:NSDateFormatterNoStyle];

  // TODO(crbug.com/694423): Make it RTL friendly.
  self.publisherLabel.text =
      [NSString stringWithFormat:@"%@ - %@.", publisherName, dateString];
}

#pragma mark - UIView

// Implements -layoutSubviews as per instructions in documentation for
// +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
- (void)layoutSubviews {
  [super layoutSubviews];

  // Adjust the text label preferredMaxLayoutWidth when the parent's width
  // changes, for instance on screen rotation.
  CGFloat parentWidth = CGRectGetWidth(self.contentView.bounds);
  self.subtitleLabel.preferredMaxLayoutWidth =
      parentWidth - self.imageView.bounds.size.width - 3 * 8;

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

@end
