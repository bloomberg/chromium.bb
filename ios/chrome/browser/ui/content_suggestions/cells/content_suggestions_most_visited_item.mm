// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_item.h"

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_tile.h"
#import "ios/chrome/browser/ui/favicon/favicon_attributes.h"
#import "ios/chrome/browser/ui/favicon/favicon_view.h"
#include "ios/chrome/browser/ui/ui_util.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kSpacingIPhone = 16;
const CGFloat kSpacingIPad = 24;
}

#pragma mark - ContentSuggestionsMostVisitedItem

@implementation ContentSuggestionsMostVisitedItem

@synthesize suggestionIdentifier = _suggestionIdentifier;
@synthesize suggestions = _suggestions;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsMostVisitedCell class];
  }
  return self;
}

- (void)configureCell:(ContentSuggestionsMostVisitedCell*)cell {
  [super configureCell:cell];
  [cell setSuggestions:self.suggestions];
}

@end

#pragma mark - ContentSuggestionsMostVisitedCell

@interface ContentSuggestionsMostVisitedCell ()

// The Most Visited tiles to be displayed.
@property(nonatomic, strong)
    NSMutableArray<ContentSuggestionsMostVisitedTile*>* tiles;

// The first line of Most Visited tiles.
@property(nonatomic, strong) UIStackView* firstLine;

// The second line of Most Visited tiles, displayed below the first one.
@property(nonatomic, strong) UIStackView* secondLine;

// Contains both stack views.
@property(nonatomic, copy) NSArray<UIStackView*>* stackViews;

// Superview for the stack views, used to center them.
@property(nonatomic, strong) UIView* stackContainer;

// Width of the |stackContainer|, allowing resizing.
@property(nonatomic, strong) NSLayoutConstraint* containerWidth;

// Number of tiles per line during the previous layout.
@property(nonatomic, assign) NSUInteger previousNumberOfTilesPerLine;

@end

@implementation ContentSuggestionsMostVisitedCell : MDCCollectionViewCell

@synthesize tiles = _tiles;
@synthesize firstLine = _firstLine;
@synthesize secondLine = _secondLine;
@synthesize stackViews = _stackViews;
@synthesize stackContainer = _stackContainer;
@synthesize containerWidth = _containerWidth;
@synthesize previousNumberOfTilesPerLine = _previousNumberOfTilesPerLine;

#pragma mark - Public

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _tiles = [NSMutableArray array];
    _firstLine = [[UIStackView alloc] init];
    _secondLine = [[UIStackView alloc] init];
    _stackViews = @[ _firstLine, _secondLine ];
    _stackContainer = [[UIView alloc] init];
    _previousNumberOfTilesPerLine = 0;

    for (UIStackView* row in self.stackViews) {
      row.axis = UILayoutConstraintAxisHorizontal;
      row.spacing = [self spacing];
      row.translatesAutoresizingMaskIntoConstraints = NO;
      [_stackContainer addSubview:row];
      row.layoutMarginsRelativeArrangement = YES;
    }

    _stackContainer.translatesAutoresizingMaskIntoConstraints = NO;

    [self.contentView addSubview:_stackContainer];

    [_stackContainer.centerXAnchor
        constraintEqualToAnchor:self.contentView.centerXAnchor]
        .active = YES;
    _containerWidth = [_stackContainer.widthAnchor constraintEqualToConstant:0];
    _containerWidth.active = YES;

    ApplyVisualConstraints(@[ @"V:|[container]|" ],
                           @{ @"container" : _stackContainer });
    ApplyVisualConstraints(
        @[
          @"V:|[first][second]|", @"H:|[first]-(>=0)-|",
          @"H:|[second]-(>=0)-|"
        ],
        @{ @"first" : _firstLine,
           @"second" : _secondLine });
  }
  return self;
}

- (void)setSuggestions:(NSArray<ContentSuggestionsMostVisited*>*)suggestions {
  [self.tiles removeAllObjects];
  for (ContentSuggestionsMostVisited* suggestion in suggestions) {
    [self.tiles addObject:[ContentSuggestionsMostVisitedTile
                              tileWithTitle:suggestion.title
                                 attributes:suggestion.attributes]];
  }
}

- (NSUInteger)numberOfTilesPerLine {
  CGFloat availableWidth = self.contentView.bounds.size.width;

  if (availableWidth > [self widthForNumberOfItem:4])
    return 4;
  if (availableWidth > [self widthForNumberOfItem:3])
    return 3;
  if (availableWidth > [self widthForNumberOfItem:2])
    return 2;

  NOTREACHED();
  return 2;
}

#pragma mark - UIView

// Implements -layoutSubviews as per instructions in documentation for
// +[MDCCollectionViewCell cr_preferredHeightForWidth:forItem:].
- (void)layoutSubviews {
  [super layoutSubviews];

  [self applyLayout];

  [super layoutSubviews];
}

#pragma mark - Private

// Layouts the tiles. The view is modified only if the number of tiles per line
// changes.
- (void)applyLayout {
  NSUInteger numberOfTilesPerLine = [self numberOfTilesPerLine];

  if (numberOfTilesPerLine == self.previousNumberOfTilesPerLine) {
    return;
  }
  self.previousNumberOfTilesPerLine = numberOfTilesPerLine;

  for (UIStackView* row in self.stackViews) {
    while (row.arrangedSubviews.count > 0) {
      UIView* view = row.arrangedSubviews.firstObject;
      [row removeArrangedSubview:view];
      [view removeFromSuperview];
    }
  }

  NSUInteger numberOfTilesFirstLine =
      MIN(numberOfTilesPerLine, self.tiles.count);
  for (NSUInteger i = 0; i < numberOfTilesFirstLine; i++) {
    [self.firstLine addArrangedSubview:self.tiles[i]];
  }
  if (self.tiles.count > numberOfTilesPerLine) {
    NSUInteger totalNumberOfTiles =
        MIN(2 * numberOfTilesPerLine, self.tiles.count);
    for (NSUInteger i = numberOfTilesPerLine; i < totalNumberOfTiles; i++) {
      [self.secondLine addArrangedSubview:self.tiles[i]];
    }
  }

  self.containerWidth.constant =
      [self widthForNumberOfItem:numberOfTilesPerLine];
}

// Returns the spacing between tiles, based on the device.
- (CGFloat)spacing {
  return IsIPadIdiom() ? kSpacingIPad : kSpacingIPhone;
}

// Returns the width necessary to fit |numberOfItem| items.
- (CGFloat)widthForNumberOfItem:(NSUInteger)numberOfItem {
  return (numberOfItem - 1) * [self spacing] +
         numberOfItem * [ContentSuggestionsMostVisitedTile width];
}

@end
