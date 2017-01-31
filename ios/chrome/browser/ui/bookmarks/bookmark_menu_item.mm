// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_menu_item.h"

#include "base/hash.h"
#include "base/logging.h"
#include "base/mac/objc_property_releaser.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_node.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

using bookmarks::BookmarkNode;

namespace bookmarks {
BOOL NumberIsValidMenuItemType(int number) {
  // Invalid and deprecated numbers.
  if (number < 1 || number > MenuItemLast)
    return NO;
  MenuItemType type = static_cast<MenuItemType>(number);
  switch (type) {
    case MenuItemFolder:
      return YES;

    case MenuItemDivider:
    case MenuItemSectionHeader:
      return NO;
  }
}
}  // namespace bookmarks

@interface BookmarkMenuItem () {
  base::mac::ObjCPropertyReleaser _propertyReleaser_BookmarkMenuItem;
}
// Redefined to be read-write.
@property(nonatomic, assign) const BookmarkNode* folder;
@property(nonatomic, assign) const BookmarkNode* rootAncestor;
// Redefined to be read-write.
@property(nonatomic, assign) bookmarks::MenuItemType type;
// Redefined to be read-write.
@property(nonatomic, copy) NSString* sectionTitle;
@end

@implementation BookmarkMenuItem
@synthesize folder = _folder;
@synthesize rootAncestor = _rootAncestor;
@synthesize sectionTitle = _sectionTitle;
@synthesize type = _type;

- (instancetype)init {
  self = [super init];
  if (self) {
    _propertyReleaser_BookmarkMenuItem.Init(self, [BookmarkMenuItem class]);
  }
  return self;
}

- (UIAccessibilityTraits)accessibilityTraits {
  switch (self.type) {
    case bookmarks::MenuItemFolder:
      return super.accessibilityTraits |= UIAccessibilityTraitButton;
    case bookmarks::MenuItemSectionHeader:
      return super.accessibilityTraits |= UIAccessibilityTraitHeader;
    case bookmarks::MenuItemDivider:
      return UIAccessibilityTraitNone;
  }
}

- (NSString*)title {
  switch (self.type) {
    case bookmarks::MenuItemDivider:
      return nil;
    case bookmarks::MenuItemFolder:
      return bookmark_utils_ios::TitleForBookmarkNode(self.folder);
    case bookmarks::MenuItemSectionHeader:
      return self.sectionTitle;
  }
}

- (NSString*)titleForMenu {
  switch (self.type) {
    case bookmarks::MenuItemDivider:
    case bookmarks::MenuItemFolder:
    case bookmarks::MenuItemSectionHeader:
      return [self title];
  }
}

- (NSString*)titleForNavigationBar {
  switch (self.type) {
    case bookmarks::MenuItemDivider:
    case bookmarks::MenuItemFolder:
    case bookmarks::MenuItemSectionHeader:
      return [self title];
  }
}

- (NSString*)accessibilityIdentifier {
  switch (self.type) {
    case bookmarks::MenuItemDivider:
      return nil;
    case bookmarks::MenuItemFolder:
      return @"MenuItemFolder";
    case bookmarks::MenuItemSectionHeader:
      return @"MenuItemSectionHeader";
  }
}

- (UIImage*)imagePrimary:(BOOL)primary {
  switch (self.type) {
    case bookmarks::MenuItemFolder:
      if (self.folder->type() == BookmarkNode::BOOKMARK_BAR ||
          self.folder->type() == BookmarkNode::MOBILE ||
          self.folder->type() == BookmarkNode::OTHER_NODE) {
        if (primary)
          return [UIImage imageNamed:@"bookmark_blue_folder"];
        else
          return [UIImage imageNamed:@"bookmark_gray_folder"];
      }
    case bookmarks::MenuItemDivider:
    case bookmarks::MenuItemSectionHeader:
      return nil;
  }
}

- (CGFloat)height {
  if (self.type == bookmarks::MenuItemDivider)
    return 1;
  return 48;
}

- (BOOL)canBeSelected {
  switch (self.type) {
    case bookmarks::MenuItemDivider:
    case bookmarks::MenuItemSectionHeader:
      return NO;
    case bookmarks::MenuItemFolder:
      return YES;
  }
}

- (BOOL)supportsEditing {
  switch (self.type) {
    case bookmarks::MenuItemFolder:
      return YES;
    case bookmarks::MenuItemDivider:
    case bookmarks::MenuItemSectionHeader:
      NOTREACHED();
      return NO;
  }
}

- (BOOL)isEqual:(id)other {
  if (other == self)
    return YES;
  if (!other || ![other isKindOfClass:[self class]])
    return NO;
  BookmarkMenuItem* otherMenuItem = static_cast<BookmarkMenuItem*>(other);
  if (self.type != otherMenuItem.type)
    return NO;

  switch (self.type) {
    case bookmarks::MenuItemDivider:
      return YES;
    case bookmarks::MenuItemFolder:
      return self.folder == otherMenuItem.folder;
    case bookmarks::MenuItemSectionHeader:
      return self.sectionTitle == otherMenuItem.sectionTitle;
  }
}

- (BookmarkMenuItem*)parentItem {
  if (self.type != bookmarks::MenuItemFolder)
    return self;
  BookmarkMenuItem* item = [[[BookmarkMenuItem alloc] init] autorelease];
  item.type = self.type;
  item.folder = self.rootAncestor;
  item.rootAncestor = self.rootAncestor;
  return item;
}

- (NSUInteger)hash {
  switch (self.type) {
    case bookmarks::MenuItemDivider:
      return self.type;
    case bookmarks::MenuItemFolder:
      return self.type + reinterpret_cast<NSUInteger>(self.folder);
    case bookmarks::MenuItemSectionHeader:
      return self.type + [self.sectionTitle hash];
  }
}

+ (BookmarkMenuItem*)dividerMenuItem {
  BookmarkMenuItem* item = [[[BookmarkMenuItem alloc] init] autorelease];
  item.type = bookmarks::MenuItemDivider;
  return item;
}

+ (BookmarkMenuItem*)folderMenuItemForNode:(const BookmarkNode*)node
                              rootAncestor:(const BookmarkNode*)ancestor {
  BookmarkMenuItem* item = [[[BookmarkMenuItem alloc] init] autorelease];
  item.type = bookmarks::MenuItemFolder;
  item.folder = node;
  item.rootAncestor = ancestor;
  return item;
}

+ (BookmarkMenuItem*)sectionMenuItemWithTitle:(NSString*)title {
  BookmarkMenuItem* item = [[[BookmarkMenuItem alloc] init] autorelease];
  item.type = bookmarks::MenuItemSectionHeader;
  item.sectionTitle = title;
  return item;
}

@end
