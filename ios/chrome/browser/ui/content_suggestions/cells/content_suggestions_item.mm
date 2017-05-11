// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_item.h"

#include "base/time/time.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/favicon/favicon_attributes.h"
#import "ios/chrome/browser/ui/favicon/favicon_view.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/i18n_string.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kImageSize = 72;
const CGFloat kStandardSpacing = 16;
const CGFloat kSmallSpacing = 8;

// Name of the icon displayed when a suggestion is available offline.
NSString* const kOfflineIconName = @"content_suggestions_offline";
// Size of the icon displayed when a suggestion is available offline.
const CGFloat kOfflineIconSize = 24;

// Size of the favicon view.
const CGFloat kFaviconSize = 16;
// Size of the icon displayed when there is not image but one should be
// displayed.
const CGFloat kIconSize = 24;
// Name of the icon displayed when there is not image but one should be
// displayed.
NSString* const kNoImageIconName = @"content_suggestions_no_image";
// No image icon percentage of white.
const CGFloat kNoImageIconWhite = 0.38;
// No image background percentage of white.
const CGFloat kNoImageBackgroundWhite = 0.95;
// Duration of the animation to display the image.
const CGFloat kAnimationDuration = 0.3;
}

@interface ContentSuggestionsItem ()

@property(nonatomic, copy) NSString* subtitle;
// Used to check if the image has already been fetched. There is no way to
// discriminate between failed image download and nonexitent image. The
// suggestion tries to download the image only once.
@property(nonatomic, assign) BOOL imageFetched;

@end

#pragma mark - ContentSuggestionsItem

@implementation ContentSuggestionsItem

@synthesize title = _title;
@synthesize subtitle = _subtitle;
@synthesize image = _image;
@synthesize URL = _URL;
@synthesize publisher = _publisher;
@synthesize publishDate = _publishDate;
@synthesize suggestionIdentifier = _suggestionIdentifier;
@synthesize delegate = _delegate;
@synthesize imageFetched = _imageFetched;
@synthesize attributes = _attributes;
@synthesize hasImage = _hasImage;
@synthesize availableOffline = _availableOffline;

- (instancetype)initWithType:(NSInteger)type
                       title:(NSString*)title
                    subtitle:(NSString*)subtitle
                    delegate:(id<ContentSuggestionsItemDelegate>)delegate
                         url:(const GURL&)url {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsCell class];
    _title = [title copy];
    _subtitle = [subtitle copy];
    _URL = url;
    _delegate = delegate;
  }
  return self;
}

- (void)configureCell:(ContentSuggestionsCell*)cell {
  [super configureCell:cell];
  if (self.hasImage && !self.imageFetched) {
    self.imageFetched = YES;
    // Fetch the image. During the fetch the cell's image should still be set.
    [self.delegate loadImageForSuggestionItem:self];
  }
  [cell.faviconView configureWithAttributes:self.attributes];
  cell.titleLabel.text = self.title;
  [cell setSubtitleText:self.subtitle];
  cell.displayImage = self.hasImage;
  [cell setContentImage:self.image];
  [cell setAdditionalInformationWithPublisherName:self.publisher
                                             date:self.publishDate
                              offlineAvailability:self.availableOffline];
}

@end

#pragma mark - ContentSuggestionsCell

@interface ContentSuggestionsCell ()

@property(nonatomic, strong) UILabel* additionalInformationLabel;
// Contains the no-image icon or the image.
@property(nonatomic, strong) UIView* imageContainer;
// The no-image icon displayed when there is no image.
@property(nonatomic, strong) UIImageView* noImageIcon;
// Displays the image associated with this suggestion. It is added to the
// imageContainer only if there is an image to display, hiding the no-image
// icon.
@property(nonatomic, strong) UIImageView* contentImageView;
// Constraint for the size of the image.
@property(nonatomic, strong) NSLayoutConstraint* imageSize;
// Constraint for the distance between the texts and the image.
@property(nonatomic, strong) NSLayoutConstraint* imageTitleSpacing;
// Constraint for the vertical distance between the title and the subtitle.
@property(nonatomic, strong) NSLayoutConstraint* titleSubtitleSpacing;
// Label for the subtitle.
@property(nonatomic, readonly, strong) UILabel* subtitleLabel;

// Applies the constraints on the elements. Called in the init.
- (void)applyConstraints;

@end

@implementation ContentSuggestionsCell

@synthesize titleLabel = _titleLabel;
@synthesize subtitleLabel = _subtitleLabel;
@synthesize imageContainer = _imageContainer;
@synthesize noImageIcon = _noImageIcon;
@synthesize additionalInformationLabel = _additionalInformationLabel;
@synthesize contentImageView = _contentImageView;
@synthesize faviconView = _faviconView;
@synthesize imageSize = _imageSize;
@synthesize imageTitleSpacing = _imageTitleSpacing;
@synthesize displayImage = _displayImage;
@synthesize titleSubtitleSpacing = _titleSubtitleSpacing;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _titleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _subtitleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _imageContainer = [[UIView alloc] initWithFrame:CGRectZero];
    _noImageIcon = [[UIImageView alloc] initWithFrame:CGRectZero];
    _additionalInformationLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _contentImageView = [[UIImageView alloc] initWithFrame:CGRectZero];
    _faviconView = [[FaviconViewNew alloc] init];

    _titleLabel.numberOfLines = 2;
    _subtitleLabel.numberOfLines = 2;
    [_subtitleLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                      forAxis:UILayoutConstraintAxisVertical];
    [_titleLabel setContentHuggingPriority:UILayoutPriorityDefaultHigh
                                   forAxis:UILayoutConstraintAxisVertical];

    _contentImageView.contentMode = UIViewContentModeScaleAspectFill;
    _contentImageView.clipsToBounds = YES;
    _contentImageView.hidden = YES;

    _imageContainer.translatesAutoresizingMaskIntoConstraints = NO;
    _noImageIcon.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _additionalInformationLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _contentImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _faviconView.translatesAutoresizingMaskIntoConstraints = NO;

    [self.contentView addSubview:_imageContainer];
    [self.contentView addSubview:_titleLabel];
    [self.contentView addSubview:_subtitleLabel];
    [self.contentView addSubview:_additionalInformationLabel];
    [self.contentView addSubview:_faviconView];

    [_imageContainer addSubview:_noImageIcon];
    [_imageContainer addSubview:_contentImageView];

    _imageContainer.backgroundColor =
        [UIColor colorWithWhite:kNoImageBackgroundWhite alpha:1];
    _noImageIcon.image = [[UIImage imageNamed:kNoImageIconName]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    [_noImageIcon
        setTintColor:[UIColor colorWithWhite:kNoImageIconWhite alpha:1]];

    _titleLabel.font = [MDCTypography subheadFont];
    _subtitleLabel.font = [MDCTypography body1Font];
    _additionalInformationLabel.font = [MDCTypography captionFont];
    _faviconView.font = [[MDCTypography fontLoader] mediumFontOfSize:10];

    _subtitleLabel.textColor = [[MDCPalette greyPalette] tint700];
    _additionalInformationLabel.textColor = [[MDCPalette greyPalette] tint700];

    [self applyConstraints];
  }
  return self;
}

- (void)setContentImage:(UIImage*)image {
  if (!image) {
    self.contentImageView.hidden = YES;
    return;
  }

  self.contentImageView.image = image;

  self.contentImageView.alpha = 0;
  self.contentImageView.hidden = NO;

  [UIView animateWithDuration:kAnimationDuration
                   animations:^{
                     self.contentImageView.alpha = 1;
                   }];
}

- (void)setAdditionalInformationWithPublisherName:(NSString*)publisherName
                                             date:(base::Time)publishDate
                              offlineAvailability:(BOOL)availableOffline {
  NSDate* date = [NSDate dateWithTimeIntervalSince1970:publishDate.ToDoubleT()];
  NSString* dateString =
      [NSDateFormatter localizedStringFromDate:date
                                     dateStyle:NSDateFormatterMediumStyle
                                     timeStyle:NSDateFormatterNoStyle];

  NSString* publisherString = AdjustStringForLocaleDirection(
      [NSString stringWithFormat:@"%@ - %@ ", publisherName, dateString]);

  NSMutableAttributedString* additionInformation =
      [[NSMutableAttributedString alloc] initWithString:publisherString
                                             attributes:nil];

  if (availableOffline) {
    NSTextAttachment* offlineIcon = [[NSTextAttachment alloc] init];
    offlineIcon.image = [[UIImage imageNamed:kOfflineIconName]
        imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    offlineIcon.bounds = CGRectMake(
        0, (_additionalInformationLabel.font.xHeight - kOfflineIconSize) / 2,
        kOfflineIconSize, kOfflineIconSize);

    [additionInformation
        appendAttributedString:[NSAttributedString
                                   attributedStringWithAttachment:offlineIcon]];
  }

  self.additionalInformationLabel.attributedText = additionInformation;
}

- (void)setSubtitleText:(NSString*)subtitle {
  self.subtitleLabel.text = subtitle;
  if (subtitle.length > 0) {
    self.titleSubtitleSpacing.constant = kSmallSpacing;
  } else {
    self.titleSubtitleSpacing.constant = 0;
  }
}

- (void)setDisplayImage:(BOOL)displayImage {
  if (displayImage) {
    self.imageTitleSpacing.constant = kStandardSpacing;
    self.imageSize.constant = kImageSize;
    self.imageContainer.hidden = NO;
  } else {
    self.imageTitleSpacing.constant = 0;
    self.imageSize.constant = 0;
    self.imageContainer.hidden = YES;
  }
  _displayImage = displayImage;
}

#pragma mark - UICollectionViewCell

- (void)prepareForReuse {
  self.contentImageView.hidden = YES;
}

#pragma mark - UIView

// Implements -layoutSubviews as per instructions in documentation for
// +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
- (void)layoutSubviews {
  [super layoutSubviews];

  // Adjust the text label preferredMaxLayoutWidth when the parent's width
  // changes, for instance on screen rotation.
  CGFloat parentWidth = CGRectGetWidth(self.contentView.bounds);

  CGFloat offset = 0;
  if (self.displayImage) {
    offset = kImageSize + kStandardSpacing;
  }

  self.titleLabel.preferredMaxLayoutWidth =
      parentWidth - 2 * kStandardSpacing - offset;
  self.subtitleLabel.preferredMaxLayoutWidth =
      parentWidth - 2 * kStandardSpacing - offset;
  self.additionalInformationLabel.preferredMaxLayoutWidth =
      parentWidth - kFaviconSize - kSmallSpacing - 2 * kStandardSpacing;

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

#pragma mark - Private

- (void)applyConstraints {
  _imageSize =
      [_imageContainer.heightAnchor constraintEqualToConstant:kImageSize];
  _imageTitleSpacing = [_imageContainer.leadingAnchor
      constraintEqualToAnchor:_titleLabel.trailingAnchor
                     constant:kStandardSpacing];
  _titleSubtitleSpacing =
      [_subtitleLabel.topAnchor constraintEqualToAnchor:_titleLabel.bottomAnchor
                                               constant:kSmallSpacing];

  [NSLayoutConstraint activateConstraints:@[
    // Image.
    _imageSize,
    [_imageContainer.widthAnchor
        constraintEqualToAnchor:_imageContainer.heightAnchor],
    [_imageContainer.topAnchor constraintEqualToAnchor:_titleLabel.topAnchor],

    // Text.
    _imageTitleSpacing, _titleSubtitleSpacing,
    [_titleLabel.trailingAnchor
        constraintEqualToAnchor:_subtitleLabel.trailingAnchor],

    // Additional Information.
    [_additionalInformationLabel.topAnchor
        constraintGreaterThanOrEqualToAnchor:_imageContainer.bottomAnchor
                                    constant:kStandardSpacing],
    [_additionalInformationLabel.topAnchor
        constraintGreaterThanOrEqualToAnchor:_subtitleLabel.bottomAnchor
                                    constant:kStandardSpacing],
    [_additionalInformationLabel.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.contentView.bottomAnchor
                                 constant:-kStandardSpacing],

    // Favicon.
    [_faviconView.topAnchor
        constraintGreaterThanOrEqualToAnchor:_imageContainer.bottomAnchor
                                    constant:kStandardSpacing],
    [_faviconView.topAnchor
        constraintGreaterThanOrEqualToAnchor:_subtitleLabel.bottomAnchor
                                    constant:kStandardSpacing],
    [_faviconView.centerYAnchor
        constraintEqualToAnchor:_additionalInformationLabel.centerYAnchor],
    [_faviconView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.contentView.bottomAnchor
                                 constant:-kStandardSpacing],
    [_faviconView.heightAnchor constraintEqualToConstant:kFaviconSize],
    [_faviconView.widthAnchor
        constraintEqualToAnchor:_faviconView.heightAnchor],

    // No image icon.
    [_noImageIcon.centerXAnchor
        constraintEqualToAnchor:_imageContainer.centerXAnchor],
    [_noImageIcon.centerYAnchor
        constraintEqualToAnchor:_imageContainer.centerYAnchor],
    [_noImageIcon.widthAnchor constraintEqualToConstant:kIconSize],
    [_noImageIcon.heightAnchor constraintEqualToAnchor:_noImageIcon.widthAnchor]
  ]];

  AddSameConstraints(_contentImageView, _imageContainer);

  ApplyVisualConstraintsWithMetrics(
      @[
        @"H:|-(space)-[title]",
        @"H:[image]-(space)-|",
        @"H:|-(space)-[text]",
        @"V:|-(space)-[title]",
        @"H:|-(space)-[favicon]-(small)-[additional]-(space)-|",
      ],
      @{
        @"image" : _imageContainer,
        @"title" : _titleLabel,
        @"text" : _subtitleLabel,
        @"additional" : _additionalInformationLabel,
        @"favicon" : _faviconView,
      },
      @{ @"space" : @(kStandardSpacing),
         @"small" : @(kSmallSpacing) });
}

@end
