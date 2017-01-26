// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"

namespace {
typedef NSMutableArray<CollectionViewItem*> SectionItems;
}

@implementation CollectionViewModel {
  // Ordered list of section identifiers, one per section in the model.
  base::scoped_nsobject<NSMutableArray<NSNumber*>> _sectionIdentifiers;

  // The lists of section items, one per section.
  base::scoped_nsobject<NSMutableArray<SectionItems*>> _sections;

  // Maps from section identifier to header and footer.
  base::scoped_nsobject<NSMutableDictionary<NSNumber*, CollectionViewItem*>>
      _headers;
  base::scoped_nsobject<NSMutableDictionary<NSNumber*, CollectionViewItem*>>
      _footers;
}

- (instancetype)init {
  if ((self = [super init])) {
    _sectionIdentifiers.reset([[NSMutableArray alloc] init]);
    _sections.reset([[NSMutableArray alloc] init]);
    _headers.reset([[NSMutableDictionary alloc] init]);
    _footers.reset([[NSMutableDictionary alloc] init]);
  }
  return self;
}

#pragma mark Modification methods

- (void)addSectionWithIdentifier:(NSInteger)sectionIdentifier {
  DCHECK_GE(sectionIdentifier, kSectionIdentifierEnumZero);
  DCHECK_EQ(static_cast<NSUInteger>(NSNotFound),
            [self internalSectionForIdentifier:sectionIdentifier]);
  [_sectionIdentifiers addObject:@(sectionIdentifier)];

  base::scoped_nsobject<SectionItems> section([[SectionItems alloc] init]);
  [_sections addObject:section];
}

- (void)insertSectionWithIdentifier:(NSInteger)sectionIdentifier
                            atIndex:(NSUInteger)index {
  DCHECK_GE(sectionIdentifier, kSectionIdentifierEnumZero);
  DCHECK_EQ(static_cast<NSUInteger>(NSNotFound),
            [self internalSectionForIdentifier:sectionIdentifier]);
  DCHECK_LE(index, [_sections count]);

  [_sectionIdentifiers insertObject:@(sectionIdentifier) atIndex:index];

  base::scoped_nsobject<SectionItems> section([[SectionItems alloc] init]);
  [_sections insertObject:section atIndex:index];
}

- (void)addItem:(CollectionViewItem*)item
    toSectionWithIdentifier:(NSInteger)sectionIdentifier {
  DCHECK_GE(item.type, kItemTypeEnumZero);
  NSInteger section = [self sectionForSectionIdentifier:sectionIdentifier];
  SectionItems* items = [_sections objectAtIndex:section];
  [items addObject:item];
}

- (void)insertItem:(CollectionViewItem*)item
    inSectionWithIdentifier:(NSInteger)sectionIdentifier
                    atIndex:(NSUInteger)index {
  DCHECK_GE(item.type, kItemTypeEnumZero);
  NSInteger section = [self sectionForSectionIdentifier:sectionIdentifier];
  SectionItems* items = [_sections objectAtIndex:section];
  DCHECK(index <= [items count]);
  [items insertObject:item atIndex:index];
}

- (void)removeItemWithType:(NSInteger)itemType
    fromSectionWithIdentifier:(NSInteger)sectionIdentifier {
  [self removeItemWithType:itemType
      fromSectionWithIdentifier:sectionIdentifier
                        atIndex:0];
}

- (void)removeItemWithType:(NSInteger)itemType
    fromSectionWithIdentifier:(NSInteger)sectionIdentifier
                      atIndex:(NSUInteger)index {
  NSInteger section = [self sectionForSectionIdentifier:sectionIdentifier];
  SectionItems* items = [_sections objectAtIndex:section];
  NSInteger item =
      [self itemForItemType:itemType inSectionItems:items atIndex:index];
  DCHECK_NE(NSNotFound, item);
  [items removeObjectAtIndex:item];
}

- (void)removeSectionWithIdentifier:(NSInteger)sectionIdentifier {
  NSInteger section = [self sectionForSectionIdentifier:sectionIdentifier];
  [_sectionIdentifiers removeObjectAtIndex:section];
  [_sections removeObjectAtIndex:section];
}

- (void)setHeader:(CollectionViewItem*)header
    forSectionWithIdentifier:(NSInteger)sectionIdentifier {
  NSNumber* key = [NSNumber numberWithInteger:sectionIdentifier];
  if (header) {
    [_headers setObject:header forKey:key];
  } else {
    [_headers removeObjectForKey:key];
  }
}

- (void)setFooter:(CollectionViewItem*)footer
    forSectionWithIdentifier:(NSInteger)sectionIdentifier {
  NSNumber* key = [NSNumber numberWithInteger:sectionIdentifier];
  if (footer) {
    [_footers setObject:footer forKey:key];
  } else {
    [_footers removeObjectForKey:key];
  }
}

#pragma mark Query model coordinates from index paths

- (NSInteger)sectionIdentifierForSection:(NSInteger)section {
  DCHECK_LT(static_cast<NSUInteger>(section), [_sectionIdentifiers count]);
  return [[_sectionIdentifiers objectAtIndex:section] integerValue];
}

- (NSInteger)itemTypeForIndexPath:(NSIndexPath*)indexPath {
  return [self itemAtIndexPath:indexPath].type;
}

- (NSUInteger)indexInItemTypeForIndexPath:(NSIndexPath*)indexPath {
  DCHECK_LT(static_cast<NSUInteger>(indexPath.section), [_sections count]);
  SectionItems* items = [_sections objectAtIndex:indexPath.section];

  CollectionViewItem* item = [self itemAtIndexPath:indexPath];
  NSUInteger indexInItemType =
      [self indexInItemTypeForItem:item inSectionItems:items];
  return indexInItemType;
}

#pragma mark Query items from index paths

- (BOOL)hasItemAtIndexPath:(NSIndexPath*)indexPath {
  if (!indexPath)
    return NO;

  if (static_cast<NSUInteger>(indexPath.section) < [_sections count]) {
    SectionItems* items = [_sections objectAtIndex:indexPath.section];
    return static_cast<NSUInteger>(indexPath.item) < [items count];
  }
  return NO;
}

- (CollectionViewItem*)itemAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK(indexPath);
  DCHECK_LT(static_cast<NSUInteger>(indexPath.section), [_sections count]);
  SectionItems* items = [_sections objectAtIndex:indexPath.section];

  DCHECK_LT(static_cast<NSUInteger>(indexPath.item), [items count]);
  return [items objectAtIndex:indexPath.item];
}

- (CollectionViewItem*)headerForSection:(NSInteger)section {
  NSInteger sectionIdentifier = [self sectionIdentifierForSection:section];
  NSNumber* key = [NSNumber numberWithInteger:sectionIdentifier];
  return [_headers objectForKey:key];
}

- (CollectionViewItem*)footerForSection:(NSInteger)section {
  NSInteger sectionIdentifier = [self sectionIdentifierForSection:section];
  NSNumber* key = [NSNumber numberWithInteger:sectionIdentifier];
  return [_footers objectForKey:key];
}

- (NSArray*)itemsInSectionWithIdentifier:(NSInteger)sectionIdentifier {
  NSInteger section = [self sectionForSectionIdentifier:sectionIdentifier];
  DCHECK_LT(static_cast<NSUInteger>(section), [_sections count]);
  return [_sections objectAtIndex:section];
}

- (CollectionViewItem*)headerForSectionWithIdentifier:
    (NSInteger)sectionIdentifier {
  NSNumber* key = [NSNumber numberWithInteger:sectionIdentifier];
  return [_headers objectForKey:key];
}

- (CollectionViewItem*)footerForSectionWithIdentifier:
    (NSInteger)sectionIdentifier {
  NSNumber* key = [NSNumber numberWithInteger:sectionIdentifier];
  return [_footers objectForKey:key];
}

#pragma mark Query index paths from model coordinates

- (BOOL)hasSectionForSectionIdentifier:(NSInteger)sectionIdentifier {
  NSUInteger section = [self internalSectionForIdentifier:sectionIdentifier];
  return section != static_cast<NSUInteger>(NSNotFound);
}

- (NSInteger)sectionForSectionIdentifier:(NSInteger)sectionIdentifier {
  NSUInteger section = [self internalSectionForIdentifier:sectionIdentifier];
  DCHECK_NE(static_cast<NSUInteger>(NSNotFound), section);
  return section;
}

- (BOOL)hasItemForItemType:(NSInteger)itemType
         sectionIdentifier:(NSInteger)sectionIdentifier {
  return [self hasItemForItemType:itemType
                sectionIdentifier:sectionIdentifier
                          atIndex:0];
}

- (NSIndexPath*)indexPathForItemType:(NSInteger)itemType
                   sectionIdentifier:(NSInteger)sectionIdentifier {
  return [self indexPathForItemType:itemType
                  sectionIdentifier:sectionIdentifier
                            atIndex:0];
}

- (BOOL)hasItemForItemType:(NSInteger)itemType
         sectionIdentifier:(NSInteger)sectionIdentifier
                   atIndex:(NSUInteger)index {
  if (![self hasSectionForSectionIdentifier:sectionIdentifier]) {
    return NO;
  }
  NSInteger section = [self sectionForSectionIdentifier:sectionIdentifier];
  SectionItems* items = [_sections objectAtIndex:section];
  NSInteger item =
      [self itemForItemType:itemType inSectionItems:items atIndex:index];
  return item != NSNotFound;
}

- (NSIndexPath*)indexPathForItemType:(NSInteger)itemType
                   sectionIdentifier:(NSInteger)sectionIdentifier
                             atIndex:(NSUInteger)index {
  NSInteger section = [self sectionForSectionIdentifier:sectionIdentifier];
  SectionItems* items = [_sections objectAtIndex:section];
  NSInteger item =
      [self itemForItemType:itemType inSectionItems:items atIndex:index];
  return [NSIndexPath indexPathForItem:item inSection:section];
}

#pragma mark Query index paths from items

- (BOOL)hasItem:(CollectionViewItem*)item
    inSectionWithIdentifier:(NSInteger)sectionIdentifier {
  return [[self itemsInSectionWithIdentifier:sectionIdentifier]
             indexOfObject:item] != NSNotFound;
}

- (NSIndexPath*)indexPathForItem:(CollectionViewItem*)item
         inSectionWithIdentifier:(NSInteger)sectionIdentifier {
  NSArray* itemsInSection =
      [self itemsInSectionWithIdentifier:sectionIdentifier];

  NSInteger section = [self sectionForSectionIdentifier:sectionIdentifier];
  NSInteger itemIndex = [itemsInSection indexOfObject:item];
  DCHECK_NE(NSNotFound, itemIndex);
  return [NSIndexPath indexPathForItem:itemIndex inSection:section];
}

#pragma mark UICollectionView data sourcing

- (NSInteger)numberOfSections {
  return [_sections count];
}

- (NSInteger)numberOfItemsInSection:(NSInteger)section {
  DCHECK_LT(static_cast<NSUInteger>(section), [_sections count]);
  SectionItems* items = [_sections objectAtIndex:section];
  return items.count;
}

#pragma mark Private methods

// Returns the section for the given section identifier. If the section
// identifier is not found, NSNotFound is returned.
- (NSUInteger)internalSectionForIdentifier:(NSInteger)sectionIdentifier {
  return [_sectionIdentifiers indexOfObject:@(sectionIdentifier)];
}

// Returns the item for the given item type in the list of items, at the
// given index. If no item is found with the given type, NSNotFound is returned.
- (NSUInteger)itemForItemType:(NSInteger)itemType
               inSectionItems:(SectionItems*)sectionItems
                      atIndex:(NSUInteger)index {
  __block NSUInteger item = NSNotFound;
  __block NSUInteger indexInItemType = 0;
  [sectionItems enumerateObjectsUsingBlock:^(CollectionViewItem* obj,
                                             NSUInteger idx, BOOL* stop) {
    if (obj.type == itemType) {
      if (indexInItemType == index) {
        item = idx;
        *stop = YES;
      } else {
        indexInItemType++;
      }
    }
  }];
  return item;
}

// Returns |item|'s index among all the items of the same type in the given
// section items.  |item| must belong to |sectionItems|.
- (NSUInteger)indexInItemTypeForItem:(CollectionViewItem*)item
                      inSectionItems:(SectionItems*)sectionItems {
  DCHECK([sectionItems containsObject:item]);
  BOOL found = NO;
  NSUInteger indexInItemType = 0;
  for (CollectionViewItem* sectionItem in sectionItems) {
    if (sectionItem == item) {
      found = YES;
      break;
    }
    if (sectionItem.type == item.type) {
      indexInItemType++;
    }
  }
  DCHECK(found);
  return indexInItemType;
}

@end
