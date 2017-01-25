// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_expandable_item.h"

#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kImageSize = 80;
const CGFloat kStandardSpacing = 8;
}

#pragma mark - SuggestionsExpandableItem

@implementation SuggestionsExpandableItem {
  NSString* _title;
  NSString* _subtitle;
  UIImage* _image;
  NSString* _detail;
}

@synthesize delegate = _delegate;
@synthesize expanded = _expanded;

- (instancetype)initWithType:(NSInteger)type
                       title:(NSString*)title
                    subtitle:(NSString*)subtitle
                       image:(UIImage*)image
                  detailText:(NSString*)detail {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsExpandableCell class];
    _title = [title copy];
    _subtitle = [subtitle copy];
    _image = image;
    _detail = [detail copy];
  }
  return self;
}

#pragma mark - CollectionViewItem

- (void)configureCell:(ContentSuggestionsExpandableCell*)cell {
  [super configureCell:cell];
  cell.delegate = self.delegate;
  cell.titleLabel.text = _title;
  cell.subtitleLabel.text = _subtitle;
  cell.imageView.image = _image;
  cell.detailLabel.text = _detail;
  if (self.expanded)
    [cell expand];
  else
    [cell collapse];
}

@end

#pragma mark - ContentSuggestionsExpandableCell

@interface ContentSuggestionsExpandableCell () {
  UIView* _articleContainer;
  UIButton* _interactionButton;
  UIButton* _expandButton;
  BOOL _expanded;
  NSArray<NSLayoutConstraint*>* _expandedConstraints;
  NSArray<NSLayoutConstraint*>* _collapsedConstraints;
}

// Callback for the UI action showing the details.
- (void)expand:(id)sender;
// Callback for the UI action hiding the details.
- (void)collapse:(id)sender;

@end

@implementation ContentSuggestionsExpandableCell

@synthesize titleLabel = _titleLabel;
@synthesize subtitleLabel = _subtitleLabel;
@synthesize detailLabel = _detailLabel;
@synthesize imageView = _imageView;
@synthesize delegate = _delegate;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _expanded = NO;
    _titleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _subtitleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _imageView = [[UIImageView alloc] initWithFrame:CGRectZero];
    _articleContainer = [[UIView alloc] initWithFrame:CGRectZero];
    _expandButton = [UIButton buttonWithType:UIButtonTypeSystem];
    _detailLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _interactionButton = [UIButton buttonWithType:UIButtonTypeSystem];

    _subtitleLabel.numberOfLines = 0;
    [_expandButton setTitle:@"See more" forState:UIControlStateNormal];
    _detailLabel.numberOfLines = 0;
    [_interactionButton setTitle:@"Less interaction"
                        forState:UIControlStateNormal];

    _imageView.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _articleContainer.translatesAutoresizingMaskIntoConstraints = NO;
    _expandButton.translatesAutoresizingMaskIntoConstraints = NO;
    _detailLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _interactionButton.translatesAutoresizingMaskIntoConstraints = NO;

    [_expandButton addTarget:self
                      action:@selector(expand:)
            forControlEvents:UIControlEventTouchUpInside];
    [_interactionButton addTarget:self
                           action:@selector(collapse:)
                 forControlEvents:UIControlEventTouchUpInside];

    [_articleContainer addSubview:_imageView];
    [_articleContainer addSubview:_titleLabel];
    [_articleContainer addSubview:_subtitleLabel];

    [self.contentView addSubview:_articleContainer];
    [self.contentView addSubview:_expandButton];

    [NSLayoutConstraint activateConstraints:@[
      [self.contentView.centerXAnchor
          constraintEqualToAnchor:_expandButton.centerXAnchor],
      [_expandButton.topAnchor
          constraintEqualToAnchor:_articleContainer.bottomAnchor],
      [_expandButton.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor],
      [_articleContainer.bottomAnchor
          constraintGreaterThanOrEqualToAnchor:_imageView.bottomAnchor
                                      constant:kStandardSpacing],
      [_articleContainer.bottomAnchor
          constraintGreaterThanOrEqualToAnchor:_subtitleLabel.bottomAnchor
                                      constant:kStandardSpacing],
    ]];

    ApplyVisualConstraintsWithMetrics(
        @[
          @"H:|[container]|", @"H:|-[title]-[image(imageSize)]-|",
          @"H:|-[text]-[image]", @"V:|-[image(imageSize)]",
          @"V:|-[title]-[text]", @"V:|[container]"
        ],
        @{
          @"image" : _imageView,
          @"title" : _titleLabel,
          @"text" : _subtitleLabel,
          @"container" : _articleContainer
        },
        @{ @"imageSize" : @(kImageSize) });

    _expandedConstraints = @[
      [_detailLabel.topAnchor
          constraintEqualToAnchor:_articleContainer.bottomAnchor],
      [_detailLabel.bottomAnchor
          constraintEqualToAnchor:_interactionButton.topAnchor],
      [_interactionButton.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor],
      [_detailLabel.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor],
      [_detailLabel.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor]
    ];
    _collapsedConstraints = @[
      [self.contentView.centerXAnchor
          constraintEqualToAnchor:_expandButton.centerXAnchor],
      [_expandButton.topAnchor
          constraintEqualToAnchor:_articleContainer.bottomAnchor],
      [_expandButton.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor]
    ];
  }
  return self;
}

#pragma mark - Private

- (void)expand:(id)sender {
  [self.delegate expandCell:self];
}

- (void)collapse:(id)sender {
  [self.delegate collapseCell:self];
}

- (void)expand {
  if (_expanded)
    return;
  _expanded = YES;

  [self.contentView addSubview:_detailLabel];
  [self.contentView addSubview:_interactionButton];
  [_expandButton removeFromSuperview];

  [NSLayoutConstraint deactivateConstraints:_collapsedConstraints];
  [NSLayoutConstraint activateConstraints:_expandedConstraints];
}

- (void)collapse {
  if (!_expanded)
    return;
  _expanded = NO;

  [_detailLabel removeFromSuperview];
  [_interactionButton removeFromSuperview];
  [self.contentView addSubview:_expandButton];

  [NSLayoutConstraint deactivateConstraints:_expandedConstraints];
  [NSLayoutConstraint activateConstraints:_collapsedConstraints];
}

#pragma mark - UIView

// Implements -layoutSubviews as per instructions in documentation for
// +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
- (void)layoutSubviews {
  [super layoutSubviews];

  // Adjust the text label preferredMaxLayoutWidth when the parent's width
  // changes, for instance on screen rotation.
  CGFloat parentWidth = CGRectGetWidth(self.contentView.bounds);
  _subtitleLabel.preferredMaxLayoutWidth = parentWidth - kImageSize - 3 * 8;
  _detailLabel.preferredMaxLayoutWidth = parentWidth;

  // Re-layout with the new preferred width to allow the label to adjust its
  // height.
  [super layoutSubviews];
}

@end
