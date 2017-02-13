// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_favicon_item.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_favicon_internal_cell.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Space between two cells in the internal collection view.
const CGFloat kInternalCellSpacing = 40;
// Width of the cells in the internal collection view.
const CGFloat kInternalCellWidth = 70;
// Height of the cells in the internal collection view.
const CGFloat kInternalCellHeight = 120;
// Leading inset of the internal collection view.
const CGFloat kInternalLeadingSpacing = 16;
}  // namespace

#pragma mark - SugggestionsFaviconItem

// The item is the data source of the inner collection view.
@interface ContentSuggestionsFaviconItem ()<UICollectionViewDataSource> {
  NSMutableArray<NSString*>* _faviconTitles;
  NSMutableArray<UIImage*>* _faviconImages;
}

@end

@implementation ContentSuggestionsFaviconItem

@synthesize delegate = _delegate;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsFaviconCell class];
    _faviconTitles = [NSMutableArray array];
    _faviconImages = [NSMutableArray array];
  }
  return self;
}

- (void)addFavicon:(UIImage*)favicon withTitle:(NSString*)title {
  if (!favicon || !title)
    return;

  [_faviconImages addObject:favicon];
  [_faviconTitles addObject:title];
}

#pragma mark - UICollectionViewDataSource

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  ContentSuggestionsFaviconInternalCell* cell = [collectionView
      dequeueReusableCellWithReuseIdentifier:
          [ContentSuggestionsFaviconInternalCell reuseIdentifier]
                                forIndexPath:indexPath];
  cell.faviconView.image = [_faviconImages objectAtIndex:indexPath.item];
  cell.titleLabel.text = [_faviconTitles objectAtIndex:indexPath.item];
  return cell;
}

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  DCHECK([_faviconImages count] == [_faviconTitles count]);
  return [_faviconImages count];
}

#pragma mark - CollectionViewItem

- (void)configureCell:(ContentSuggestionsFaviconCell*)cell {
  [super configureCell:cell];
  cell.collectionView.dataSource = self;
  cell.delegate = self.delegate;
}

@end

#pragma mark - ContentSuggestionsFaviconCell

// The cell is the delegate of the inner collection view.
@interface ContentSuggestionsFaviconCell ()<UICollectionViewDelegate>

@end

@implementation ContentSuggestionsFaviconCell

@synthesize collectionView = _collectionView;
@synthesize delegate = _delegate;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    UICollectionViewFlowLayout* layout =
        [[UICollectionViewFlowLayout alloc] init];
    layout.scrollDirection = UICollectionViewScrollDirectionHorizontal;
    layout.minimumInteritemSpacing = kInternalCellSpacing;
    layout.itemSize = CGSizeMake(kInternalCellWidth, kInternalCellHeight);
    layout.sectionInset = UIEdgeInsetsMake(0, kInternalLeadingSpacing, 0, 0);

    _collectionView = [[UICollectionView alloc] initWithFrame:CGRectZero
                                         collectionViewLayout:layout];
    [_collectionView registerClass:[ContentSuggestionsFaviconInternalCell class]
        forCellWithReuseIdentifier:[ContentSuggestionsFaviconInternalCell
                                       reuseIdentifier]];
    _collectionView.backgroundColor = [UIColor clearColor];
    _collectionView.translatesAutoresizingMaskIntoConstraints = NO;

    _collectionView.delegate = self;

    [self.contentView addSubview:_collectionView];
    AddSameSizeConstraint(_collectionView, self.contentView);
    [_collectionView.heightAnchor constraintEqualToConstant:kInternalCellHeight]
        .active = YES;
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.collectionView.dataSource = nil;
  self.delegate = nil;
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  [self.delegate openFaviconAtIndexPath:indexPath];
}

@end
