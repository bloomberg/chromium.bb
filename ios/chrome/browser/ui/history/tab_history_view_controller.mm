// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/tab_history_view_controller.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/history/tab_history_cell.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#import "ios/third_party/material_components_ios/src/components/Ink/src/MaterialInk.h"
#import "ios/web/navigation/crw_session_entry.h"
#include "ios/web/public/favicon_status.h"
#include "ios/web/public/navigation_item.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Visible percentage of the last visible row on the Tools menu if the
// Tools menu is scrollable.
const CGFloat kLastRowVisiblePercentage = 0.6;
// Reuse identifier for cells.
NSString* cellIdentifier = @"TabHistoryCell";
NSString* footerIdentifier = @"Footer";
NSString* headerIdentifier = @"Header";
// Height of rows.
const CGFloat kCellHeight = 48.0;
// Fraction height for partially visible row.
const CGFloat kCellHeightLastRow = kCellHeight * kLastRowVisiblePercentage;
// Width and leading for the header view.
const CGFloat kHeaderLeadingInset = 0;
const CGFloat kHeaderWidth = 48;
// Leading and trailing insets for cell items.
const CGFloat kCellLeadingInset = kHeaderLeadingInset + kHeaderWidth;
const CGFloat kCellTrailingInset = 16;

typedef std::vector<CGFloat> CGFloatVector;
typedef std::vector<CGFloatVector> ItemOffsetVector;

NS_INLINE CGFloat OffsetForPath(const ItemOffsetVector& offsets,
                                NSInteger section,
                                NSInteger item) {
  DCHECK(section < (NSInteger)offsets.size());
  DCHECK(item < (NSInteger)offsets.at(section).size());

  return offsets.at(section).at(item);
}

NS_INLINE CGFloat OffsetForPath(const ItemOffsetVector& offsets,
                                NSIndexPath* path) {
  return OffsetForPath(offsets, [path section], [path item]);
}

NS_INLINE CGFloat OffsetForSection(const ItemOffsetVector& offsets,
                                   NSIndexPath* path) {
  DCHECK([path section] < (NSInteger)offsets.size());
  DCHECK(offsets.at([path section]).size());

  return offsets.at([path section]).at(0);
}

// Height for the footer view.
NS_INLINE CGFloat FooterHeight() {
  return 1.0 / [[UIScreen mainScreen] scale];
}

}  // namespace

@interface TabHistoryViewControllerLayout : UICollectionViewLayout
@end

@implementation TabHistoryViewControllerLayout {
  ItemOffsetVector _itemYOffsets;
  CGFloat _containerCalculatedHeight;
  CGFloat _containerWidth;
  CGFloat _cellItemWidth;
  CGFloat _footerWidth;
}

- (void)prepareLayout {
  [super prepareLayout];

  UICollectionView* collectionView = [self collectionView];
  CGFloat yOffset = 0;

  NSInteger numberOfSections = [collectionView numberOfSections];
  _itemYOffsets.reserve(numberOfSections);

  for (NSInteger section = 0; section < numberOfSections; ++section) {
    NSInteger numberOfItems = [collectionView numberOfItemsInSection:section];

    CGFloatVector dummy;
    _itemYOffsets.push_back(dummy);

    CGFloatVector& sectionYOffsets = _itemYOffsets.at(section);
    sectionYOffsets.reserve(numberOfItems);

    for (NSInteger item = 0; item < numberOfItems; ++item) {
      sectionYOffsets.push_back(yOffset);
      yOffset += kCellHeight;
    }

    // The last section should not have a footer.
    if (numberOfItems && (section + 1) < numberOfSections) {
      yOffset += FooterHeight();
    }
  }

  CGRect containerBounds = [collectionView bounds];
  _containerWidth = CGRectGetWidth(containerBounds);
  _cellItemWidth = _containerWidth - kCellLeadingInset - kCellTrailingInset;
  _footerWidth = _containerWidth - kCellLeadingInset;
  _containerCalculatedHeight = yOffset - kCellHeight / 2.0;
}

- (void)invalidateLayout {
  [super invalidateLayout];
  _itemYOffsets.clear();
  _containerCalculatedHeight = 0;
  _cellItemWidth = 0;
  _footerWidth = 0;
}

- (CGSize)collectionViewContentSize {
  return CGSizeMake(_containerWidth, _containerCalculatedHeight);
}

- (NSArray*)layoutAttributesForElementsInRect:(CGRect)rect {
  UICollectionView* collectionView = [self collectionView];
  NSMutableArray* array = [NSMutableArray array];

  NSInteger numberOfSections = [collectionView numberOfSections];
  for (NSInteger section = 0; section < numberOfSections; ++section) {
    NSInteger numberOfItems = [collectionView numberOfItemsInSection:section];
    if (numberOfItems) {
      NSIndexPath* path =
          [NSIndexPath indexPathForItem:numberOfItems - 1 inSection:section];
      [array addObject:[self layoutAttributesForSupplementaryViewOfKind:
                                 UICollectionElementKindSectionHeader
                                                            atIndexPath:path]];
    }

    for (NSInteger item = 0; item < numberOfItems; ++item) {
      NSIndexPath* path = [NSIndexPath indexPathForItem:item inSection:section];
      [array addObject:[self layoutAttributesForItemAtIndexPath:path]];
    }

    // The last section should not have a footer.
    if (numberOfItems && (section + 1) < numberOfSections) {
      NSIndexPath* path =
          [NSIndexPath indexPathForItem:numberOfItems - 1 inSection:section];
      [array addObject:[self layoutAttributesForSupplementaryViewOfKind:
                                 UICollectionElementKindSectionFooter
                                                            atIndexPath:path]];
    }
  }

  return array;
}

- (UICollectionViewLayoutAttributes*)layoutAttributesForItemAtIndexPath:
    (NSIndexPath*)indexPath {
  CGFloat yOffset = OffsetForPath(_itemYOffsets, indexPath);

  UICollectionViewLayoutAttributes* attributes =
      [UICollectionViewLayoutAttributes
          layoutAttributesForCellWithIndexPath:indexPath];
  LayoutRect cellLayout = LayoutRectMake(kCellLeadingInset, _containerWidth,
                                         yOffset, _cellItemWidth, kCellHeight);
  [attributes setFrame:LayoutRectGetRect(cellLayout)];
  [attributes setZIndex:1];

  return attributes;
}

- (UICollectionViewLayoutAttributes*)
layoutAttributesForSupplementaryViewOfKind:(NSString*)kind
                               atIndexPath:(NSIndexPath*)indexPath {
  CGFloat yOffset = OffsetForPath(_itemYOffsets, indexPath);

  UICollectionViewLayoutAttributes* attributes =
      [UICollectionViewLayoutAttributes
          layoutAttributesForSupplementaryViewOfKind:kind
                                       withIndexPath:indexPath];

  if ([kind isEqualToString:UICollectionElementKindSectionHeader]) {
    // The height is the yOffset of the first section minus the yOffset of the
    // last item. Additionally, the height of a cell needs to be added back in
    // since the _itemYOffsets stores Mid Y.
    CGFloat headerYOffset = OffsetForSection(_itemYOffsets, indexPath);
    CGFloat height = yOffset - headerYOffset + kCellHeight;

    if ([indexPath section])
      headerYOffset += FooterHeight();

    LayoutRect cellLayout = LayoutRectMake(kHeaderLeadingInset, _containerWidth,
                                           headerYOffset, kHeaderWidth, height);
    [attributes setFrame:LayoutRectGetRect(cellLayout)];
    [attributes setZIndex:0];
  } else if ([kind isEqualToString:UICollectionElementKindSectionFooter]) {
    LayoutRect cellLayout =
        LayoutRectMake(kCellLeadingInset, _containerWidth, yOffset,
                       _footerWidth, FooterHeight());
    [attributes setFrame:LayoutRectGetRect(cellLayout)];
    [attributes setZIndex:0];
  }

  return attributes;
}

@end

@interface TabHistoryViewController ()<MDCInkTouchControllerDelegate> {
  MDCInkTouchController* _inkTouchController;
  NSArray* _partitionedEntries;
  NSArray* _sessionEntries;
}
@end

@implementation TabHistoryViewController

- (NSArray*)sessionEntries {
  return _sessionEntries;
}

#pragma mark Public Methods

- (CGFloat)optimalHeight:(CGFloat)suggestedHeight {
  DCHECK(suggestedHeight >= kCellHeight);
  CGFloat optimalHeight = 0;

  for (NSArray* sectionArray in _partitionedEntries) {
    NSUInteger sectionItemCount = [sectionArray count];
    for (NSUInteger i = 0; i < sectionItemCount; ++i) {
      CGFloat proposedHeight = optimalHeight + kCellHeight;

      if (proposedHeight > suggestedHeight) {
        CGFloat difference = proposedHeight - suggestedHeight;
        if (difference > kCellHeightLastRow) {
          return optimalHeight + kCellHeightLastRow;
        } else {
          return optimalHeight - kCellHeight + kCellHeightLastRow;
        }
      }

      optimalHeight = proposedHeight;
    }

    optimalHeight += FooterHeight();
  }

  // If this point is reached, it means the entire content fits and this last
  // section should not include the footer.
  optimalHeight -= FooterHeight();

  return optimalHeight;
}

- (instancetype)init {
  TabHistoryViewControllerLayout* layout =
      [[TabHistoryViewControllerLayout alloc] init];

  return [self initWithCollectionViewLayout:layout];
}

- (instancetype)initWithCollectionViewLayout:(UICollectionViewLayout*)layout {
  self = [super initWithCollectionViewLayout:layout];
  if (self) {
    UICollectionView* collectionView = [self collectionView];
    [collectionView setBackgroundColor:[UIColor whiteColor]];

    [collectionView registerClass:[TabHistoryCell class]
        forCellWithReuseIdentifier:cellIdentifier];

    [collectionView registerClass:[TabHistorySectionHeader class]
        forSupplementaryViewOfKind:UICollectionElementKindSectionHeader
               withReuseIdentifier:headerIdentifier];

    [collectionView registerClass:[TabHistorySectionFooter class]
        forSupplementaryViewOfKind:UICollectionElementKindSectionFooter
               withReuseIdentifier:footerIdentifier];

    _inkTouchController =
        [[MDCInkTouchController alloc] initWithView:collectionView];
    [_inkTouchController setDelegate:self];
    [_inkTouchController addInkView];
  }

  return self;
}

#pragma mark UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  UICollectionViewCell* cell =
      [collectionView cellForItemAtIndexPath:indexPath];
  [collectionView chromeExecuteCommand:cell];
}

#pragma mark UICollectionViewDataSource

- (CRWSessionEntry*)entryForIndexPath:(NSIndexPath*)indexPath {
  NSInteger section = [indexPath section];
  NSInteger item = [indexPath item];

  DCHECK(section < (NSInteger)[_partitionedEntries count]);
  DCHECK(item < (NSInteger)[[_partitionedEntries objectAtIndex:section] count]);
  NSArray* sectionedArray = [_partitionedEntries objectAtIndex:section];

  return [sectionedArray objectAtIndex:item];
}

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  DCHECK(section < (NSInteger)[_partitionedEntries count]);
  return [[_partitionedEntries objectAtIndex:section] count];
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  TabHistoryCell* cell =
      [collectionView dequeueReusableCellWithReuseIdentifier:cellIdentifier
                                                forIndexPath:indexPath];

  [cell setEntry:[self entryForIndexPath:indexPath]];
  [cell setTag:IDC_BACK_FORWARD_IN_TAB_HISTORY];

  return cell;
}

- (NSInteger)numberOfSectionsInCollectionView:(UICollectionView*)view {
  return [_partitionedEntries count];
}

- (UICollectionReusableView*)collectionView:(UICollectionView*)view
          viewForSupplementaryElementOfKind:(NSString*)kind
                                atIndexPath:(NSIndexPath*)indexPath {
  if ([kind isEqualToString:UICollectionElementKindSectionFooter]) {
    return [view dequeueReusableSupplementaryViewOfKind:kind
                                    withReuseIdentifier:footerIdentifier
                                           forIndexPath:indexPath];
  }

  DCHECK([kind isEqualToString:UICollectionElementKindSectionHeader]);
  CRWSessionEntry* sessionEntry = [self entryForIndexPath:indexPath];
  web::NavigationItem* navigationItem = [sessionEntry navigationItem];

  TabHistorySectionHeader* header =
      [view dequeueReusableSupplementaryViewOfKind:kind
                               withReuseIdentifier:headerIdentifier
                                      forIndexPath:indexPath];

  UIImage* iconImage = nil;
  const gfx::Image& image = navigationItem->GetFavicon().image;
  if (!image.IsEmpty())
    iconImage = image.ToUIImage();
  else
    iconImage = [UIImage imageNamed:@"default_favicon"];

  [[header iconView] setImage:iconImage];

  return header;
}

- (void)setSessionEntries:(NSArray*)sessionEntries {
  _sessionEntries = sessionEntries;

  std::string previousHost;

  NSMutableArray* sectionArray = [NSMutableArray array];
  NSMutableArray* partitionedEntries = [NSMutableArray array];

  NSInteger numberOfEntries = [_sessionEntries count];
  for (NSInteger index = 0; index < numberOfEntries; ++index) {
    CRWSessionEntry* sessionEntry = [_sessionEntries objectAtIndex:index];
    web::NavigationItem* navigationItem = [sessionEntry navigationItem];

    std::string currentHost;
    if (navigationItem)
      currentHost = navigationItem->GetURL().host();

    if (previousHost.empty())
      previousHost = currentHost;

    // TODO: This should use some sort of Top Level Domain matching instead of
    // explicit host match so that images.googe.com matches shopping.google.com.
    if (previousHost == currentHost) {
      [sectionArray addObject:sessionEntry];
    } else {
      [partitionedEntries addObject:sectionArray];
      sectionArray = [NSMutableArray arrayWithObject:sessionEntry];
      previousHost = currentHost;
    }
  }

  if ([sectionArray count])
    [partitionedEntries addObject:sectionArray];

  if (![partitionedEntries count])
    partitionedEntries = nil;

  _partitionedEntries = partitionedEntries;
}

#pragma mark MDCInkTouchControllerDelegate

- (BOOL)inkTouchController:(MDCInkTouchController*)inkTouchController
    shouldProcessInkTouchesAtTouchLocation:(CGPoint)location {
  NSIndexPath* indexPath =
      [self.collectionView indexPathForItemAtPoint:location];
  TabHistoryCell* cell = base::mac::ObjCCastStrict<TabHistoryCell>(
      [self.collectionView cellForItemAtIndexPath:indexPath]);
  if (!cell) {
    return NO;
  }

  // Increase the size of the ink view to cover the collection view from edge
  // to edge.
  CGRect inkViewFrame = [cell frame];
  inkViewFrame.origin.x = 0;
  inkViewFrame.size.width = CGRectGetWidth([self.collectionView bounds]);
  [[inkTouchController defaultInkView] setFrame:inkViewFrame];
  return YES;
}

@end
