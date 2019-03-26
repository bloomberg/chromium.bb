// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_row_cell.h"

#import "ios/chrome/browser/ui/omnibox/popup/autocomplete_suggestion.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_truncating_label.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kLeadingMargin = 24;
const CGFloat kVerticalSpace = 6;
}  // namespace

@interface OmniboxPopupRowCell ()

@property(nonatomic, strong) OmniboxPopupTruncatingLabel* textTruncatingLabel;
@property(nonatomic, strong) OmniboxPopupTruncatingLabel* detailTruncatingLabel;

@property(nonatomic, assign) BOOL incognito;

@end

@implementation OmniboxPopupRowCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    _textTruncatingLabel =
        [[OmniboxPopupTruncatingLabel alloc] initWithFrame:CGRectZero];
    _detailTruncatingLabel =
        [[OmniboxPopupTruncatingLabel alloc] initWithFrame:CGRectZero];

    _incognito = NO;

    self.backgroundColor = [UIColor clearColor];
  }
  return self;
}

- (void)setupLayout {
  self.textTruncatingLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentView addSubview:self.textTruncatingLabel];
  self.detailTruncatingLabel.translatesAutoresizingMaskIntoConstraints = NO;
  [self.contentView addSubview:self.detailTruncatingLabel];

  UILayoutGuide* safeAreaLayoutGuide = self.contentView.safeAreaLayoutGuide;

  [NSLayoutConstraint activateConstraints:@[
    [self.textTruncatingLabel.leadingAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.leadingAnchor
                       constant:kLeadingMargin],
    [self.textTruncatingLabel.trailingAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.trailingAnchor],
    [self.textTruncatingLabel.topAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.topAnchor
                       constant:kVerticalSpace],

    [self.detailTruncatingLabel.leadingAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.leadingAnchor
                       constant:kLeadingMargin],
    [self.detailTruncatingLabel.trailingAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.trailingAnchor],
    [self.detailTruncatingLabel.topAnchor
        constraintEqualToAnchor:self.textTruncatingLabel.bottomAnchor
                       constant:kVerticalSpace],
    [self.detailTruncatingLabel.bottomAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.bottomAnchor],
  ]];
}

- (void)prepareForReuse {
  [super prepareForReuse];

  self.incognito = NO;
}

- (void)setupWithAutocompleteSuggestion:(id<AutocompleteSuggestion>)suggestion
                              incognito:(BOOL)incognito {
  // Setup the view layout the first time the cell is setup.
  if (self.contentView.subviews.count == 0) {
    [self setupLayout];
  }
  self.incognito = incognito;

  self.textTruncatingLabel.attributedText = suggestion.text;
  self.detailTruncatingLabel.attributedText = suggestion.detailText;
}

@end
