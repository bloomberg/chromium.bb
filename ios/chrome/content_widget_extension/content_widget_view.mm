// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/content_widget_extension/content_widget_view.h"

#import "ios/chrome/browser/ui/favicon/favicon_view.h"
#import "ios/chrome/browser/ui/util/constraints_ui_util.h"
#import "ios/chrome/content_widget_extension/most_visited_tile_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Spacing between tiles.
const CGFloat kTileSpacing = 16;
// Height of a tile row.
const CGFloat kTileHeight = 100;
// Icons to show per row.
const int kIconsPerRow = 4;
}

@interface ContentWidgetView ()

// The first row of sites.
@property(nonatomic, strong) UIView* firstRow;
// The second row of sites.
@property(nonatomic, strong) UIView* secondRow;
// The height used in the compact display mode.
@property(nonatomic) CGFloat compactHeight;
// The first row's height constraint. Set its constant to modify the first row's
// height.
@property(nonatomic, strong) NSLayoutConstraint* firstRowHeightConstraint;
// Whether the second row of sites should be shown. False if there are no sites
// to show in that row.
@property(nonatomic, readonly) BOOL shouldShowSecondRow;
// The number of sites to display.
@property(nonatomic, assign) int siteCount;

// Sets up the widget UI for an expanded or compact appearance based on
// |compact|.
- (void)createUI:(BOOL)compact;

// Creates the view for a row of 4 sites.
- (UIView*)createRowOfSites;

// Returns the height to use for the first row, depending on the display mode.
- (CGFloat)firstRowHeight:(BOOL)compact;

// Returns the height to use for the second row (can be 0 if the row should not
// be shown).
- (CGFloat)secondRowHeight;

@end

@implementation ContentWidgetView

@synthesize firstRow = _firstRow;
@synthesize secondRow = _secondRow;
@synthesize compactHeight = _compactHeight;
@synthesize firstRowHeightConstraint = _firstRowHeightConstraint;
@synthesize siteCount = _siteCount;

- (instancetype)initWithCompactHeight:(CGFloat)compactHeight
                     initiallyCompact:(BOOL)compact {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _compactHeight = compactHeight;
    [self createUI:compact];
  }
  return self;
}

#pragma mark - properties

- (CGFloat)widgetExpandedHeight {
  return [self firstRowHeight:NO] + [self secondRowHeight];
}

- (BOOL)shouldShowSecondRow {
  return self.siteCount > kIconsPerRow;
}

#pragma mark - UI creation

- (void)createUI:(BOOL)compact {
  _firstRow = [self createRowOfSites];
  _secondRow = [self createRowOfSites];

  [self addSubview:_firstRow];
  [self addSubview:_secondRow];

  _firstRowHeightConstraint = [_firstRow.heightAnchor
      constraintEqualToConstant:[self firstRowHeight:compact]];

  [NSLayoutConstraint activateConstraints:@[
    [_firstRow.topAnchor constraintEqualToAnchor:self.topAnchor],
    [_secondRow.topAnchor constraintEqualToAnchor:_firstRow.bottomAnchor],
    [self.leadingAnchor constraintEqualToAnchor:_firstRow.leadingAnchor],
    [self.leadingAnchor constraintEqualToAnchor:_secondRow.leadingAnchor],
    [self.trailingAnchor constraintEqualToAnchor:_firstRow.trailingAnchor],
    [self.trailingAnchor constraintEqualToAnchor:_secondRow.trailingAnchor],
    _firstRowHeightConstraint,
  ]];
}

- (UIView*)createRowOfSites {
  NSMutableArray<MostVisitedTileView*>* cells = [[NSMutableArray alloc] init];
  for (int i = 0; i < kIconsPerRow; i++) {
    cells[i] = [[MostVisitedTileView alloc] init];
    cells[i].translatesAutoresizingMaskIntoConstraints = NO;
  }

  UIStackView* stack = [[UIStackView alloc] initWithArrangedSubviews:cells];
  stack.translatesAutoresizingMaskIntoConstraints = NO;
  stack.axis = UILayoutConstraintAxisHorizontal;
  stack.alignment = UIStackViewAlignmentTop;
  stack.distribution = UIStackViewDistributionEqualSpacing;
  stack.layoutMargins = UIEdgeInsetsZero;
  stack.spacing = kTileSpacing;
  stack.layoutMarginsRelativeArrangement = YES;

  UIView* container = [[UIView alloc] initWithFrame:CGRectZero];
  container.translatesAutoresizingMaskIntoConstraints = NO;
  [container addSubview:stack];

  [NSLayoutConstraint activateConstraints:@[
    [stack.centerYAnchor constraintEqualToAnchor:container.centerYAnchor],
    [stack.centerXAnchor constraintEqualToAnchor:container.centerXAnchor],
    [container.heightAnchor constraintGreaterThanOrEqualToConstant:kTileHeight],
  ]];

  return container;
}

- (CGFloat)firstRowHeight:(BOOL)compact {
  if (compact) {
    return self.compactHeight;
  }

  CGFloat firstRowHeight = kTileHeight + 2 * kTileSpacing;
  CGFloat secondRowHeight = [self secondRowHeight];
  CGFloat totalHeight = firstRowHeight + secondRowHeight;
  if (totalHeight >= self.compactHeight) {
    return firstRowHeight;
  }

  return self.compactHeight - secondRowHeight;
}

- (CGFloat)secondRowHeight {
  return self.shouldShowSecondRow ? kTileHeight + kTileSpacing : 0;
}

#pragma mark - ContentWidgetView

- (void)showMode:(BOOL)compact {
  self.firstRowHeightConstraint.constant = [self firstRowHeight:compact];
}

@end
