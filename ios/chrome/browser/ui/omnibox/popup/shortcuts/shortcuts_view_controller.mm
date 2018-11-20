// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/shortcuts/shortcuts_view_controller.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/ntp_tile_views/ntp_most_visited_tile_view.h"
#import "ios/chrome/browser/ui/ntp_tile_views/ntp_shortcut_tile_view.h"
#import "ios/chrome/browser/ui/ntp_tile_views/ntp_tile_constants.h"
#import "ios/chrome/browser/ui/omnibox/popup/shortcuts/collection_shortcut_cell.h"
#import "ios/chrome/browser/ui/omnibox/popup/shortcuts/most_visited_shortcut_cell.h"
#import "ios/chrome/browser/ui/omnibox/popup/shortcuts/shortcuts_view_controller_delegate.h"
#import "ios/chrome/common/favicon/favicon_view.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const NSInteger kNumberOfItemsPerRow = 4;
const CGFloat kLineSpacing = 30;
const CGFloat kItemSpacing = 10;
const CGFloat kTopInset = 10;

const NSInteger kMostVisitedSection = 0;
const NSInteger kCollectionShortcutSection = 1;
}  // namespace

@interface ShortcutsViewController ()<UICollectionViewDelegate,
                                      UICollectionViewDataSource>

@property(nonatomic, strong) UICollectionViewFlowLayout* layout;
@property(nonatomic, strong) UICollectionView* collectionView;
// Latest most visited items. Updated directly from the consumer calls.
@property(nonatomic, strong)
    NSArray<ShortcutsMostVisitedItem*>* latestMostVisitedItems;
// Currently displayed most visited items. Will be set to nil when the view
// disappears, and set to |latestMostVisitedItems| when the view appears. This
// prevents the updates when the user sees the tiles.
@property(nonatomic, strong)
    NSArray<ShortcutsMostVisitedItem*>* displayedMostVisitedItems;
@property(nonatomic, assign) NSInteger readingListBadgeValue;

@end

@implementation ShortcutsViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  [self.view addSubview:self.collectionView];
  self.collectionView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.view, self.collectionView);
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  // Calculate insets to center the items in the view.
  CGFloat widthInsets = (self.view.bounds.size.width -
                         kMostVisitedCellSize.width * kNumberOfItemsPerRow -
                         kItemSpacing * (kNumberOfItemsPerRow - 1)) /
                        2;
  self.layout.sectionInset =
      UIEdgeInsetsMake(kTopInset, widthInsets, 0, widthInsets);
  // Promote the latest most visited items to the displayed ones and reload the
  // collection view data.
  self.displayedMostVisitedItems = self.latestMostVisitedItems;
  [self.collectionView reloadData];
}

#pragma mark - properties

- (UICollectionView*)collectionView {
  if (_collectionView) {
    return _collectionView;
  }

  _collectionView = [[UICollectionView alloc] initWithFrame:self.view.frame
                                       collectionViewLayout:self.layout];
  _collectionView.delegate = self;
  _collectionView.dataSource = self;
  _collectionView.backgroundColor = [UIColor clearColor];
  [_collectionView registerClass:[MostVisitedShortcutCell class]
      forCellWithReuseIdentifier:NSStringFromClass(
                                     [MostVisitedShortcutCell class])];
  [_collectionView registerClass:[CollectionShortcutCell class]
      forCellWithReuseIdentifier:NSStringFromClass(
                                     [CollectionShortcutCell class])];

  return _collectionView;
}

- (UICollectionViewFlowLayout*)layout {
  if (_layout) {
    return _layout;
  }

  _layout = [[UICollectionViewFlowLayout alloc] init];
  _layout.minimumLineSpacing = kLineSpacing;
  _layout.itemSize = kMostVisitedCellSize;
  return _layout;
}

#pragma mark - ShortcutsConsumer

- (void)mostVisitedShortcutsAvailable:
    (NSArray<ShortcutsMostVisitedItem*>*)items {
  self.latestMostVisitedItems = items;

  // Normally, the most visited tiles should not change when the user sees them.
  // However, in case there were no items, and now they're available, it is
  // better to show something, even if this means reloading the view.
  if (self.displayedMostVisitedItems.count == 0 && self.viewLoaded) {
    self.displayedMostVisitedItems = self.latestMostVisitedItems;
    [self.collectionView reloadData];
  }
}

- (void)faviconChangedForURL:(const GURL&)URL {
  if (!self.viewLoaded) {
    return;
  }

  for (ShortcutsMostVisitedItem* item in self.displayedMostVisitedItems) {
    if (item.URL == URL) {
      NSUInteger i = [self.displayedMostVisitedItems indexOfObject:item];
      NSIndexPath* indexPath =
          [NSIndexPath indexPathForItem:i inSection:kMostVisitedSection];
      MostVisitedShortcutCell* cell =
          base::mac::ObjCCastStrict<MostVisitedShortcutCell>(
              [self.collectionView cellForItemAtIndexPath:indexPath]);
      [self configureMostVisitedCell:cell
                            withItem:self.displayedMostVisitedItems[i]];
    }
  }
}

- (void)readingListBadgeUpdatedWithCount:(NSInteger)count {
  self.readingListBadgeValue = count;
  if (!self.viewLoaded) {
    return;
  }

  NSIndexPath* readingListShortcutIndexPath =
      [NSIndexPath indexPathForItem:NTPCollectionShortcutTypeReadingList
                          inSection:kCollectionShortcutSection];
  [self.collectionView
      reloadItemsAtIndexPaths:@[ readingListShortcutIndexPath ]];
}

#pragma mark - UICollectionViewDataSource

- (NSInteger)numberOfSectionsInCollectionView:
    (UICollectionView*)collectionView {
  return 2;
}

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  if (section == kMostVisitedSection) {
    return MIN(kNumberOfItemsPerRow, self.displayedMostVisitedItems.count);
  };
  return kNumberOfItemsPerRow;
}

// The cell that is returned must be retrieved from a call to
// -dequeueReusableCellWithReuseIdentifier:forIndexPath:
- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section == kMostVisitedSection) {
    MostVisitedShortcutCell* cell = [self.collectionView
        dequeueReusableCellWithReuseIdentifier:
            NSStringFromClass([MostVisitedShortcutCell class])
                                  forIndexPath:indexPath];
    ShortcutsMostVisitedItem* item =
        self.displayedMostVisitedItems[indexPath.item];
    [self configureMostVisitedCell:cell withItem:item];
    cell.accessibilityTraits = UIAccessibilityTraitButton;
    return cell;
  }

  if (indexPath.section == kCollectionShortcutSection) {
    CollectionShortcutCell* cell = [self.collectionView
        dequeueReusableCellWithReuseIdentifier:
            NSStringFromClass([CollectionShortcutCell class])
                                  forIndexPath:indexPath];
    DCHECK(indexPath.item < 4) << "Only four collection shortcuts described in "
                                  "NTPCollectionShortcutType are supported";
    NTPCollectionShortcutType type = (NTPCollectionShortcutType)indexPath.item;
    [self configureCollectionShortcutCell:cell withCollection:type];
    cell.accessibilityTraits = UIAccessibilityTraitButton;
    return cell;
  }

  return nil;
}

- (void)configureMostVisitedCell:(MostVisitedShortcutCell*)cell
                        withItem:(ShortcutsMostVisitedItem*)item {
  [cell.tile.faviconView configureWithAttributes:item.attributes];
  cell.tile.titleLabel.text = item.title;
  cell.accessibilityLabel = cell.tile.titleLabel.text;
}

- (void)configureCollectionShortcutCell:(CollectionShortcutCell*)cell
                         withCollection:(NTPCollectionShortcutType)type {
  cell.tile.titleLabel.text = TitleForCollectionShortcutType(type);
  cell.tile.iconView.image = ImageForCollectionShortcutType(type);
  cell.accessibilityLabel = cell.tile.titleLabel.text;

  if (type == NTPCollectionShortcutTypeReadingList) {
    if (self.readingListBadgeValue > 0) {
      cell.tile.countLabel.text = [@(self.readingListBadgeValue) stringValue];
      cell.tile.countContainer.hidden = NO;
      cell.accessibilityLabel = AccessibilityLabelForReadingListCellWithCount(
          self.readingListBadgeValue);
    } else {
      cell.tile.countLabel.text = nil;
    }
  }
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  if (indexPath.section == kMostVisitedSection) {
    ShortcutsMostVisitedItem* item =
        self.displayedMostVisitedItems[indexPath.item];
    DCHECK(item);
    [self.commandHandler openMostVisitedItem:item];
    base::RecordAction(
        base::UserMetricsAction("MobileOmniboxShortcutsOpenMostVisitedItem"));
  }

  if (indexPath.section == kCollectionShortcutSection) {
    NTPCollectionShortcutType type = (NTPCollectionShortcutType)indexPath.item;
    switch (type) {
      case NTPCollectionShortcutTypeBookmark:
        [self.commandHandler openBookmarks];
        base::RecordAction(
            base::UserMetricsAction("MobileOmniboxShortcutsOpenBookmarks"));
        break;
      case NTPCollectionShortcutTypeRecentTabs:
        [self.commandHandler openRecentTabs];
        base::RecordAction(
            base::UserMetricsAction("MobileOmniboxShortcutsOpenRecentTabs"));
        break;
      case NTPCollectionShortcutTypeReadingList:
        [self.commandHandler openReadingList];
        base::RecordAction(
            base::UserMetricsAction("MobileOmniboxShortcutsOpenReadingList"));
        break;
      case NTPCollectionShortcutTypeHistory:
        [self.commandHandler openHistory];
        base::RecordAction(
            base::UserMetricsAction("MobileOmniboxShortcutsOpenHistory"));
        break;
    }
  }
}

@end
