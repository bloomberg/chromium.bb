// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/cells/page_info_item.h"

#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#import "ios/third_party/material_roboto_font_loader_ios/src/src/MaterialRobotoFontLoader.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kPageInfoFaviconImageViewID = @"kPageInfoFaviconImageViewID";

namespace {
// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 12;

// Padding used on the leading and trailing edges of the cell and between the
// favicon and labels.
const CGFloat kHorizontalPadding = 16;
}

@implementation PageInfoItem

@synthesize pageFavicon = _pageFavicon;
@synthesize pageTitle = _pageTitle;
@synthesize pageHost = _pageHost;

#pragma mark CollectionViewItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [PageInfoCell class];
  }
  return self;
}

- (void)configureCell:(PageInfoCell*)cell {
  [super configureCell:cell];
  cell.pageFaviconView.image = self.pageFavicon;
  cell.pageTitleLabel.text = self.pageTitle;
  cell.pageHostLabel.text = self.pageHost;

  // Invalidate the constraints so that layout can account for whether or not a
  // favicon is present.
  [cell setNeedsUpdateConstraints];
}

@end

@implementation PageInfoCell {
  NSLayoutConstraint* _pageTitleLabelLeadingConstraint;
}

@synthesize pageTitleLabel = _pageTitleLabel;
@synthesize pageHostLabel = _pageHostLabel;
@synthesize pageFaviconView = _pageFaviconView;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.isAccessibilityElement = YES;

    // Favicon
    _pageFaviconView = [[UIImageView alloc] initWithFrame:CGRectZero];
    _pageFaviconView.translatesAutoresizingMaskIntoConstraints = NO;
    _pageFaviconView.accessibilityIdentifier = kPageInfoFaviconImageViewID;
    [self.contentView addSubview:_pageFaviconView];

    // Page title
    _pageTitleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _pageTitleLabel.font =
        [[MDFRobotoFontLoader sharedInstance] mediumFontOfSize:12];
    _pageTitleLabel.textColor = [[MDCPalette greyPalette] tint900];
    _pageTitleLabel.backgroundColor = [UIColor clearColor];
    _pageTitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_pageTitleLabel];

    // Page host
    _pageHostLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _pageHostLabel.font =
        [[MDFRobotoFontLoader sharedInstance] regularFontOfSize:12];
    _pageHostLabel.textColor = [[MDCPalette greyPalette] tint600];
    // Allow the label to break to multiple lines. This should be very rare but
    // will prevent malicious domains from suppling very long host names and
    // having the domain name truncated.
    _pageHostLabel.numberOfLines = 0;
    _pageHostLabel.backgroundColor = [UIColor clearColor];
    _pageHostLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self.contentView addSubview:_pageHostLabel];

    // Layout
    [NSLayoutConstraint activateConstraints:@[
      [_pageFaviconView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kHorizontalPadding],
      [_pageFaviconView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [_pageFaviconView.heightAnchor
          constraintEqualToAnchor:self.contentView.heightAnchor
                         constant:-(2 * kVerticalPadding)],
      [_pageFaviconView.widthAnchor
          constraintEqualToAnchor:_pageFaviconView.heightAnchor],

      // The constraint on the leading achor of the title label is activated in
      // updateConstraints rather than here so that it can depend on whether a
      // favicon is present or not.
      [_pageHostLabel.leadingAnchor
          constraintEqualToAnchor:_pageTitleLabel.leadingAnchor],

      [_pageTitleLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:self.contentView.trailingAnchor
                                   constant:-kHorizontalPadding],
      [_pageHostLabel.trailingAnchor
          constraintEqualToAnchor:_pageTitleLabel.trailingAnchor],

      // UILabel leaves some empty space above the height of capital letters. In
      // order to align the tops of the letters with the top of the favicon,
      // anchor the bottom of the label to the top of the favicon plus
      // pointSize (which describes the actual height of the letters).
      [_pageTitleLabel.bottomAnchor
          constraintEqualToAnchor:_pageFaviconView.topAnchor
                         constant:_pageTitleLabel.font.pointSize],
      [_pageHostLabel.firstBaselineAnchor
          constraintEqualToAnchor:_pageFaviconView.bottomAnchor],
    ]];
  }
  return self;
}

#pragma mark - UIView

- (void)updateConstraints {
  _pageTitleLabelLeadingConstraint.active = NO;
  _pageTitleLabelLeadingConstraint = [_pageTitleLabel.leadingAnchor
      constraintEqualToAnchor:_pageFaviconView.image
                                  ? _pageFaviconView.trailingAnchor
                                  : self.contentView.leadingAnchor
                     constant:kHorizontalPadding];
  _pageTitleLabelLeadingConstraint.active = YES;

  [super updateConstraints];
}

#pragma mark - UICollectionReusableView

- (void)prepareForReuse {
  [super prepareForReuse];
  _pageFaviconView.image = nil;
  _pageTitleLabel.text = nil;
  _pageHostLabel.text = nil;
}

#pragma mark - Accessibility

- (NSString*)accessibilityLabel {
  return [NSString stringWithFormat:@"%@, %@", self.pageTitleLabel.text,
                                    self.pageHostLabel.text];
}

@end
