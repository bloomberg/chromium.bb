// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab_collection/tab_collection_view_controller.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/clean/chrome/browser/ui/tab_collection/tab_collection_item.h"
#import "ios/clean/chrome/browser/ui/tab_collection/tab_collection_tab_cell.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabCollectionViewController ()<UICollectionViewDelegate,
                                          SessionCellDelegate>
// Collection view of tabs.
@property(nonatomic, readwrite) UICollectionView* tabs;
// The model backing the collection view.
@property(nonatomic, readwrite) NSMutableArray<TabCollectionItem*>* items;
// Selected index of tab collection.
@property(nonatomic, assign) int selectedIndex;
@end

@implementation TabCollectionViewController
@synthesize tabs = _tabs;
@synthesize items = _items;
@synthesize selectedIndex = _selectedIndex;

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  UICollectionView* tabs =
      [[UICollectionView alloc] initWithFrame:CGRectZero
                         collectionViewLayout:[self collectionViewLayout]];
  tabs.translatesAutoresizingMaskIntoConstraints = NO;
  tabs.backgroundColor = [UIColor blackColor];

  [self.view addSubview:tabs];
  self.tabs = tabs;
  self.tabs.dataSource = self;
  self.tabs.delegate = self;
  [self.tabs registerClass:[TabCollectionTabCell class]
      forCellWithReuseIdentifier:[TabCollectionTabCell identifier]];

  [NSLayoutConstraint activateConstraints:@[
    [self.tabs.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [self.tabs.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [self.tabs.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [self.tabs.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
  ]];

  [self.tabs
      selectItemAtIndexPath:[NSIndexPath indexPathForItem:self.selectedIndex
                                                inSection:0]
                   animated:NO
             scrollPosition:UICollectionViewScrollPositionNone];
}

- (UIStatusBarStyle)preferredStatusBarStyle {
  return UIStatusBarStyleLightContent;
}

#pragma mark - Setters

- (void)setSelectedIndex:(int)selectedIndex {
  [self.tabs selectItemAtIndexPath:[NSIndexPath indexPathForItem:selectedIndex
                                                       inSection:0]
                          animated:YES
                    scrollPosition:UICollectionViewScrollPositionNone];
  _selectedIndex = selectedIndex;
}

#pragma mark - Required subclass override

- (UICollectionViewLayout*)collectionViewLayout {
  NOTREACHED() << "You must override "
               << base::SysNSStringToUTF8(NSStringFromSelector(_cmd))
               << " in a subclass.";
  return nil;
}

- (void)showTabAtIndex:(int)index {
  NOTREACHED() << "You must override "
               << base::SysNSStringToUTF8(NSStringFromSelector(_cmd))
               << " in a subclass.";
}

- (void)closeTabAtIndex:(int)index {
  NOTREACHED() << "You must override "
               << base::SysNSStringToUTF8(NSStringFromSelector(_cmd))
               << " in a subclass.";
}

#pragma mark - UICollectionViewDataSource methods

- (NSInteger)numberOfSectionsInCollectionView:
    (UICollectionView*)collectionView {
  return 1;
}

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return static_cast<NSInteger>(self.items.count);
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(nonnull NSIndexPath*)indexPath {
  TabCollectionTabCell* cell =
      base::mac::ObjCCastStrict<TabCollectionTabCell>([collectionView
          dequeueReusableCellWithReuseIdentifier:[TabCollectionTabCell
                                                     identifier]
                                    forIndexPath:indexPath]);
  cell.delegate = self;
  [cell setSessionType:TabSwitcherSessionType::REGULAR_SESSION];
  DCHECK_LE(indexPath.item, INT_MAX);
  int index = static_cast<int>(indexPath.item);
  [cell setAppearanceForTabTitle:self.items[index].title
                         favicon:nil
                        cellSize:CGSizeZero];
  return cell;
}

#pragma mark - UICollectionViewDelegate methods

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  // Prevent user selection of items.
  return NO;
}

#pragma mark - SessionCellDelegate

- (TabSwitcherCache*)tabSwitcherCache {
  // PLACEHOLDER: return image cache.
  return nil;
}

- (void)cellPressed:(UICollectionViewCell*)cell {
  NSInteger item = [[self.tabs indexPathForCell:cell] item];
  DCHECK_LE(item, INT_MAX);
  int index = static_cast<int>(item);
  [self showTabAtIndex:index];
}

- (void)deleteButtonPressedForCell:(UICollectionViewCell*)cell {
  NSInteger item = [[self.tabs indexPathForCell:cell] item];
  DCHECK_LE(item, INT_MAX);
  int index = static_cast<int>(item);
  [self closeTabAtIndex:index];
}

#pragma mark - TabCollectionConsumer methods

- (void)insertItem:(TabCollectionItem*)item
           atIndex:(int)index
     selectedIndex:(int)selectedIndex {
  DCHECK_GE(index, 0);
  DCHECK_LE(static_cast<NSUInteger>(index), self.items.count);
  [self.items insertObject:item atIndex:index];
  [self.tabs insertItemsAtIndexPaths:@[ [NSIndexPath indexPathForItem:index
                                                            inSection:0] ]];
  self.selectedIndex = selectedIndex;
}

- (void)deleteItemAtIndex:(int)index selectedIndex:(int)selectedIndex {
  DCHECK_GE(index, 0);
  DCHECK_LT(static_cast<NSUInteger>(index), self.items.count);
  [self.items removeObjectAtIndex:index];
  [self.tabs deleteItemsAtIndexPaths:@[ [NSIndexPath indexPathForItem:index
                                                            inSection:0] ]];
  self.selectedIndex = selectedIndex;
}

- (void)moveItemFromIndex:(int)fromIndex
                  toIndex:(int)toIndex
            selectedIndex:(int)selectedIndex {
  TabCollectionItem* item = self.items[fromIndex];
  [self.items removeObjectAtIndex:fromIndex];
  [self.items insertObject:item atIndex:toIndex];
  [self.tabs
      moveItemAtIndexPath:[NSIndexPath indexPathForItem:fromIndex inSection:0]
              toIndexPath:[NSIndexPath indexPathForItem:toIndex inSection:0]];
  self.selectedIndex = selectedIndex;
}

- (void)replaceItemAtIndex:(int)index withItem:(TabCollectionItem*)item {
  DCHECK_GE(index, 0);
  DCHECK_LT(static_cast<NSUInteger>(index), self.items.count);
  self.items[index] = item;
  TabCollectionTabCell* cell = base::mac::ObjCCastStrict<TabCollectionTabCell>(
      [self.tabs cellForItemAtIndexPath:[NSIndexPath indexPathForItem:index
                                                            inSection:0]]);
  [cell setAppearanceForTabTitle:self.items[index].title
                         favicon:nil
                        cellSize:CGSizeZero];
}

- (void)populateItems:(NSArray<TabCollectionItem*>*)items
        selectedIndex:(int)selectedIndex {
  self.items = [items mutableCopy];
  [self.tabs reloadItemsAtIndexPaths:[self.tabs indexPathsForVisibleItems]];
  self.selectedIndex = selectedIndex;
}

@end
