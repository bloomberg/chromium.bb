// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_article_item.h"

#include "base/time/time.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/i18n_string.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kImageSize = 72;
// When updating this, make sure to update |layoutSubviews|.
const CGFloat kStandardSpacing = 8;
}

@interface ContentSuggestionsArticleItem ()

@property(nonatomic, copy) NSString* subtitle;
// Used to check if the image has already been fetched. There is no way to
// discriminate between failed image download and nonexitent image. The article
// tries to download the image only once.
@property(nonatomic, assign) BOOL imageFetched;

@end

#pragma mark - ContentSuggestionsArticleItem

@implementation ContentSuggestionsArticleItem

@synthesize title = _title;
@synthesize subtitle = _subtitle;
@synthesize image = _image;
@synthesize articleURL = _articleURL;
@synthesize publisher = _publisher;
@synthesize publishDate = _publishDate;
@synthesize suggestionIdentifier = _suggestionIdentifier;
@synthesize delegate = _delegate;
@synthesize imageFetched = _imageFetched;

- (instancetype)initWithType:(NSInteger)type
                       title:(NSString*)title
                    subtitle:(NSString*)subtitle
                    delegate:(id<ContentSuggestionsArticleItemDelegate>)delegate
                         url:(const GURL&)url {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsArticleCell class];
    _title = [title copy];
    _subtitle = [subtitle copy];
    _articleURL = url;
    _delegate = delegate;
  }
  return self;
}

- (void)configureCell:(ContentSuggestionsArticleCell*)cell {
  [super configureCell:cell];
  if (!self.image && !self.imageFetched) {
    self.imageFetched = YES;
    // Fetch the image. During the fetch the cell's image should still be set to
    // nil.
    [self.delegate loadImageForArticleItem:self];
  }
  cell.titleLabel.text = self.title;
  cell.subtitleLabel.text = self.subtitle;
  cell.imageView.image = self.image;
  [cell setPublisherName:self.publisher date:self.publishDate];
}

@end

#pragma mark - ContentSuggestionsArticleCell

@interface ContentSuggestionsArticleCell ()

@property(nonatomic, strong) UILabel* publisherLabel;

// Applies the constraints on the elements. Called in the init.
- (void)applyConstraints;

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

    _titleLabel.numberOfLines = 2;
    _subtitleLabel.numberOfLines = 0;
    [_subtitleLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                      forAxis:UILayoutConstraintAxisVertical];
    [_titleLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                   forAxis:UILayoutConstraintAxisVertical];
    _imageView.contentMode = UIViewContentModeScaleAspectFill;
    _imageView.clipsToBounds = YES;

    _imageView.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _publisherLabel.translatesAutoresizingMaskIntoConstraints = NO;

    [self.contentView addSubview:_imageView];
    [self.contentView addSubview:_titleLabel];
    [self.contentView addSubview:_subtitleLabel];
    [self.contentView addSubview:_publisherLabel];

    _titleLabel.font = [MDCTypography subheadFont];
    _subtitleLabel.font = [MDCTypography body1Font];
    _publisherLabel.font = [MDCTypography captionFont];

    _subtitleLabel.textColor = [[MDCPalette greyPalette] tint700];
    _publisherLabel.textColor = [[MDCPalette greyPalette] tint700];

    [self applyConstraints];
  }
  return self;
}

- (void)setPublisherName:(NSString*)publisherName date:(base::Time)publishDate {
  NSDate* date = [NSDate dateWithTimeIntervalSince1970:publishDate.ToDoubleT()];
  NSString* dateString =
      [NSDateFormatter localizedStringFromDate:date
                                     dateStyle:NSDateFormatterMediumStyle
                                     timeStyle:NSDateFormatterNoStyle];

  self.publisherLabel.text = AdjustStringForLocaleDirection(
      [NSString stringWithFormat:@"%@ - %@.", publisherName, dateString]);
}

#pragma mark - UIView

// Implements -layoutSubviews as per instructions in documentation for
// +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
- (void)layoutSubviews {
  [super layoutSubviews];

  // Adjust the text label preferredMaxLayoutWidth when the parent's width
  // changes, for instance on screen rotation.
  CGFloat parentWidth = CGRectGetWidth(self.contentView.bounds);

  self.titleLabel.preferredMaxLayoutWidth =
      parentWidth - self.imageView.bounds.size.width - 3 * kStandardSpacing;
  self.subtitleLabel.preferredMaxLayoutWidth =
      parentWidth - self.imageView.bounds.size.width - 3 * kStandardSpacing;

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

#pragma mark - Private

- (void)applyConstraints {
  [NSLayoutConstraint activateConstraints:@[
    [_imageView.widthAnchor constraintEqualToConstant:kImageSize],
    [_imageView.heightAnchor constraintEqualToAnchor:_imageView.widthAnchor],
    [_publisherLabel.topAnchor
        constraintGreaterThanOrEqualToAnchor:_imageView.bottomAnchor
                                    constant:kStandardSpacing],
    [_publisherLabel.topAnchor
        constraintGreaterThanOrEqualToAnchor:_subtitleLabel.bottomAnchor
                                    constant:kStandardSpacing],
  ]];

  ApplyVisualConstraintsWithMetrics(
      @[
        @"H:|-(space)-[title]-(space)-[image]-(space)-|",
        @"H:|-(space)-[text]-(space)-[image]",
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
      },
      @{ @"space" : @(kStandardSpacing) });
}

@end
