// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "bookmark_item.h"

#include "app/resource_bundle.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_window.h"
#import "chrome/browser/cocoa/bookmark_manager_controller.h"
#include "grit/app_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"


@implementation BookmarkItem

@synthesize node = node_;

- (id)initWithBookmarkNode:(const BookmarkNode*)node
                   manager:(BookmarkManagerController*)manager {
  DCHECK(manager);
  self = [super init];
  if (self) {
    node_ = node;
    manager_ = manager;
  }
  return self;
}

- (NSString*)description {
  return [NSString stringWithFormat:@"%@[%@]", [self class], [self title]];
}

- (NSString*)title {
  return base::SysWideToNSString(node_->GetTitle());
}

- (void)setTitle:(NSString*)title {
  DCHECK(![self isFixed]);
  [manager_ bookmarkModel]->SetTitle(node_, base::SysNSStringToWide(title));
}

- (NSString*)URLString {
  GURL url = node_->GetURL();
  if (url.is_empty())
    return nil;
  return base::SysUTF8ToNSString(url.possibly_invalid_spec());
}

- (void)setURLString:(NSString*)urlStr {
  //TODO(snej): Uncomment this once SetURL exists (bug 10603).
  //  ...or work around it by removing node and adding new one.
  //[manager_ bookmarkModel]->SetURL(node_, GURL([urlStr UTF8String]));
  icon_.reset(nil);
}

- (BookmarkItem*)parent {
  const BookmarkNode* parent = node_->GetParent();
  if (!parent || parent->IsRoot())
    return nil;
  return [manager_ itemFromNode:parent];
}

- (NSString*)folderPath {
  scoped_nsobject<NSMutableArray> path([[NSMutableArray alloc] init]);
  for (BookmarkItem* folder = [self parent]; folder; folder = [folder parent]) {
    [path insertObject:[folder title] atIndex:0];
  }
  return [path componentsJoinedByString:@"/"];
}

- (BOOL)isFolder {
  return node_->is_folder();
}

- (NSUInteger)numberOfChildren {
  return node_->GetChildCount();
}

- (BookmarkItem*)childAtIndex:(NSUInteger)index {
  return [manager_ itemFromNode:node_->GetChild(index)];
}

- (NSUInteger)indexOfChild:(BookmarkItem*)child {
  DCHECK(child);
  int index = node_->IndexOfChild([child node]);
  return index >= 0 ? index : NSNotFound;
}

- (BOOL)hasDescendant:(BookmarkItem*)item {
  while (item) {
    if (item == self)
      return YES;
    item = [item parent];
  }
  return NO;
}

- (BOOL)childAtIndexIsFolder:(NSUInteger)index {
  return node_->GetChild(index)->is_folder();
}

- (NSUInteger)numberOfChildFolders {
  int nChildren = [self numberOfChildren];
  NSUInteger nFolders = 0;
  for (int i = 0; i < nChildren; i++) {
    if ([self childAtIndexIsFolder:i])
      ++nFolders;
  }
  return nFolders;
}

- (BookmarkItem*)childFolderAtIndex:(NSUInteger)index {
  NSUInteger nChildren = [self numberOfChildren];
  for (NSUInteger i = 0; i < nChildren; i++) {
    if ([self childAtIndexIsFolder:i]) {
      if (index-- == 0)
        return [self childAtIndex:i];
    }
  }
  NOTREACHED();
  return nil;
}

- (NSUInteger)indexOfChildFolder:(BookmarkItem*)child {
  const BookmarkNode* childNode = [child node];
  DCHECK(childNode);
  NSUInteger folderIndex = 0;
  NSUInteger nChildren = [self numberOfChildren];
  for (NSUInteger i = 0; i < nChildren; i++) {
    if ([self childAtIndexIsFolder:i]) {
      if ([self childAtIndex:i] == child)
        return folderIndex;
      ++folderIndex;
    }
  }
  return NSNotFound;
}

+ (NSImage*)defaultIcon {
  static NSImage* sDefaultFavIcon = [ResourceBundle::GetSharedInstance().
      GetNSImageNamed(IDR_DEFAULT_FAVICON) retain];
  return sDefaultFavIcon;
}

+ (NSImage*)folderIcon {
  static NSImage* sFolderIcon = [ResourceBundle::GetSharedInstance().
      GetNSImageNamed(IDR_BOOKMARK_BAR_FOLDER) retain];
  return sFolderIcon;
}

- (NSImage*)icon {
  if (!icon_) {
    // Lazily load icon.
    NSImage* icon = [[self class] defaultIcon];
    if ([self isFolder]) {
      icon = [[self class] folderIcon];
    } else if (node_ && node_->is_url()) {
      const SkBitmap& skIcon = [manager_ bookmarkModel]->GetFavIcon(node_);
      if (!skIcon.isNull()) {
        icon = gfx::SkBitmapToNSImage(skIcon);
      }
    }
    icon_.reset([icon retain]);
  }
  return icon_;
}

- (id)persistentID {
  DCHECK(node_);
  return [NSNumber numberWithLongLong:node_->id()];
}

- (BookmarkItem*)itemWithPersistentID:(id)persistentID {
  if ([persistentID isKindOfClass:[NSNumber class]]) {
    int64 nodeID = [persistentID longLongValue];
    const BookmarkNode* node = [manager_ bookmarkModel]->GetNodeByID(nodeID);
    return [manager_ itemFromNode:node];
  }
  NOTREACHED();
  return nil;
}

- (BOOL)isFake {
  return NO;
}

- (BOOL)isFixed {
  BookmarkModel* model = [manager_ bookmarkModel];
  return node_ == model->GetBookmarkBarNode() || node_ == model->other_node();
}

// Open a bookmark, by having Chrome open a tab on its URL.
- (BOOL)open {
  if (!node_ || !node_->is_url())
    return NO;
  GURL url = node_->GetURL();

  Browser* browser = BrowserList::GetLastActive();
  // if no browser window exists then create one with no tabs to be filled in
  if (!browser) {
    browser = Browser::Create([manager_ profile]);
    browser->window()->Show();
  }
  browser->OpenURL(url, GURL(), NEW_FOREGROUND_TAB,
                   PageTransition::AUTO_BOOKMARK);
  return YES;
}

- (void)moveItem:(BookmarkItem*)item toIndex:(int)index {
  DCHECK(![item isFixed]);
  const BookmarkNode* srcNode = [item node];
  DCHECK(srcNode);
  DCHECK(node_);
  [manager_ bookmarkModel]->Move(srcNode, node_, index);
}

- (BookmarkItem*)addBookmarkWithTitle:(NSString*)title
                                  URL:(NSString*)urlString
                              atIndex:(int)index{
  DCHECK(title);
  DCHECK_GT([urlString length], 0U);
  DCHECK([self isFolder]);
  DCHECK(node_);
  BookmarkModel* model = [manager_ bookmarkModel];
  const BookmarkNode* node = model->AddURL([self node],
                                           index,
                                           base::SysNSStringToWide(title),
                                           GURL([urlString UTF8String]));
  return [manager_ itemFromNode:node];
}

- (BookmarkItem*)addFolderWithTitle:(NSString*)title
                            atIndex:(int)index {
  DCHECK(title);
  DCHECK([self isFolder]);
  DCHECK(node_);
  BookmarkModel* model = [manager_ bookmarkModel];
  const BookmarkNode* node = model->AddGroup([self node],
                                             index,
                                             base::SysNSStringToWide(title));
  return [manager_ itemFromNode:node];
}

- (BOOL)removeChild:(BookmarkItem*)child {
  DCHECK(![child isFixed]);
  if (!node_)
    return NO;
  const BookmarkNode* childNode = [child node];
  if (!childNode)
    return NO;
  int index = node_->IndexOfChild(childNode);
  if (index < 0)
    return NO;
  [manager_ bookmarkModel]->Remove(node_, index);
  return YES;
}

- (void)nodeChanged {
  icon_.reset(nil);
}

@end


@implementation FakeBookmarkItem

@synthesize title = title_, parent = parent_, children = children_;

- (id)initWithTitle:(NSString*)title
               icon:(NSImage*)icon
            manager:(BookmarkManagerController*)manager  {
  self = [super initWithBookmarkNode:nil manager:manager];
  if (self) {
    title_ = [title copy];
    children_ = [[NSMutableArray alloc] init];
    icon_.reset([icon retain]);
  }
  return self;
}

- (void)dealloc {
  [title_ release];
  [children_ release];
  [super dealloc];
}

- (BOOL)isFolder {
  return children_ != nil;
}

- (NSUInteger)numberOfChildren {
  return [children_ count];
}

- (BookmarkItem*)childAtIndex:(NSUInteger)index {
  return [children_ objectAtIndex:index];
}

- (NSUInteger)indexOfChild:(BookmarkItem*)child {
  return [children_ indexOfObjectIdenticalTo:child];
}

- (BOOL)childAtIndexIsFolder:(NSUInteger)index {
  return [[children_ objectAtIndex:index] isFolder];
}

- (NSString*)URLString {
  return nil;
}

- (void)setURLString:(NSString*)urlStr {
  NOTREACHED();
}

- (BOOL)isFake {
  return YES;
}

- (BOOL)isFixed {
  return YES;
}

@end
