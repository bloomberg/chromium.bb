// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_tree_controller.h"
#import "base/logging.h"
#import "chrome/browser/cocoa/bookmark_item.h"


// Safari uses this type, though it's not declared in any header.
static NSString* const BookmarkDictionaryListPboardType =
    @"BookmarkDictionaryListPboardType";

// Mac WebKit uses this type, declared in WebKit/mac/History/WebURLsWithTitles.h
static NSString* const WebURLsWithTitlesPboardType =
    @"WebURLsWithTitlesPboardType";

// Used internally to identify intra-outline drags.
static NSString* const kCustomPboardType =
    @"ChromeBookmarkTreeControllerPlaceholderType";

// Remembers which items are being dragged, during a drag. Static because
// there's by definition only one drag at a time, but multiple controllers.
static scoped_nsobject<NSArray> sDraggedItems;


@implementation BookmarkTreeController (Pasteboard)


// One-time dnd setup; called from -awakeFromNib.
- (void)registerDragTypes {
  [outline_ registerForDraggedTypes:[NSArray arrayWithObjects:
                                     BookmarkDictionaryListPboardType,
                                     WebURLsWithTitlesPboardType,
                                     NSURLPboardType, nil]];
  [outline_ setDraggingSourceOperationMask:NSDragOperationCopy |
                                           NSDragOperationMove
                                  forLocal:YES];
  [outline_ setDraggingSourceOperationMask:NSDragOperationCopy
                                  forLocal:NO];
}

// Selects a range of items in a parent item.
- (void)selectItemsInFolder:(BookmarkItem*)parent
                  atIndexes:(NSRange)childRange {
  DCHECK(NSMaxRange(childRange) <= (NSUInteger)[parent numberOfChildren]);
  if (parent != group_) {
    // If parent is not the root, need to offset range by parent's index:
    int startRow = [outline_ rowForItem:parent];
    if (startRow < 0) {
      return;
    }
    if ([outline_ isItemExpanded:parent]) {
      childRange.location += startRow + 1;
    } else {
      childRange.location = startRow;
      childRange.length = 1;
    }
  }
  NSIndexSet* indexes = [NSIndexSet indexSetWithIndexesInRange:childRange];
  [outline_ selectRowIndexes:indexes byExtendingSelection:NO];
}


#pragma mark -
#pragma mark DRAGGING OUT AND COPYING:


// Generates parallel arrays of URLs and titles for contents of an item.
static void flattenItem(BookmarkItem* item,
                        NSMutableArray* urlStrings,
                        NSMutableArray* titles) {
  if ([item isFolder]) {
    NSUInteger n = [item numberOfChildren];
    for (NSUInteger i = 0; i < n; i++) {
      flattenItem([item childAtIndex:i], urlStrings, titles);
    }
  } else {
    NSString* urlStr = [item URLString];
    if (urlStr) {
      [urlStrings addObject:urlStr];
      [titles addObject:[item title]];
    }
  }
}

// Writes data to the pasteboard given a list of row items.
- (BOOL)writeItems:(NSArray*)items
      toPasteboard:(NSPasteboard*)pb
           canMove:(BOOL)canMove {
  if ([items count] == 0) {
    return NO;
  }

  [pb declareTypes:[NSMutableArray arrayWithObjects:
                    WebURLsWithTitlesPboardType,
                    NSStringPboardType, nil]
             owner:self];

  // Add URLs and titles:
  NSMutableArray* urls = [NSMutableArray array];
  NSMutableArray* titles = [NSMutableArray array];
  for (BookmarkItem* item in items) {
    flattenItem(item, urls, titles);
  }
  [pb setPropertyList:[NSArray arrayWithObjects:urls, titles, nil]
              forType:WebURLsWithTitlesPboardType];

  // Add plain text, as one URL per line:
  [pb setString:[urls componentsJoinedByString:@"\n"]
        forType:NSStringPboardType];

  // If moves are allowed, remember the actual BookmarkItems in sDraggedItems,
  // and add a special pasteboard type to signal that they're there.
  if (canMove) {
    [pb addTypes:[NSArray arrayWithObject:kCustomPboardType] owner:self];
    [pb setData:[NSData data] forType:kCustomPboardType];
  }

  // Add single URL:
  if ([urls count] == 1) {
    [pb addTypes:[NSArray arrayWithObject:NSURLPboardType] owner:self];
    NSString* firstURLStr = [urls objectAtIndex:0];
    [pb setString:firstURLStr forType:NSURLPboardType];
  }
  return YES;
}

// Called after the pasteboard I've written to is no longer in use.
- (void)pasteboardChangedOwner:(NSPasteboard*)sender {
  sDraggedItems.reset(nil);
}

// Invoked when dragging outline-view rows.
- (BOOL)outlineView:(NSOutlineView*)outlineView
         writeItems:(NSArray*)items
       toPasteboard:(NSPasteboard*)pb {
  // Special items (bookmark bar, Recents...) cannot be moved.
  BOOL canMove = YES;
  for (BookmarkItem* item in items) {
    if ([item isFixed]) {
      canMove = NO;
      break;
    }
  }

  sDraggedItems.reset(canMove ? [items copy] : nil);

  [self writeItems:items toPasteboard:pb canMove:canMove];
  return YES;
}


// The Cut command.
- (IBAction)cut:(id)sender {
  if ([self writeItems:[self actionItems]
          toPasteboard:[NSPasteboard generalPasteboard]
               canMove:NO]) {
    [self delete:self];
  } else {
    NSBeep();
  }
}

// The Copy command.
- (IBAction)copy:(id)sender {
  if (![self copyToPasteboard:[NSPasteboard generalPasteboard]])
    NSBeep();
}

// Copy to any pasteboard.
- (BOOL)copyToPasteboard:(NSPasteboard*)pb {
  return [self writeItems:[self actionItems]
             toPasteboard:pb
                  canMove:NO];
}


#pragma mark -
#pragma mark INCOMING DRAGS AND PASTING:


// BookmarkDictionaryListPboardType represents bookmarks as dictionaries,
// which have the following keys.
// Strangely, folder items (whose WebBookmarkType is WebBookmarkTypeLeaf) have
// their title under 'Title', while leaf items have it in 'URIDictionary.title'.
static const NSString* kTitleKey = @"Title";
static const NSString* kURIDictionaryKey = @"URIDictionary";
static const NSString* kURIDictTitleKey = @"title";
static const NSString* kURLStringKey = @"URLString";
static const NSString* kTypeKey = @"WebBookmarkType";
static const NSString* kLeafType = @"WebBookmarkTypeLeaf";
//static const NSString* kListType = @"WebBookmarkTypeList"; // unused for now
static const NSString* kChildrenKey = @"Children";

// Helper that creates a dictionary in BookmarkDictionaryListPboardType format.
// |name| may be nil, but |urlStr| is required.
static NSDictionary* makeBookmarkPlistEntry(NSString* name, NSString* urlStr) {
  if (!name) {
    name = urlStr;
  }
  NSDictionary* nameDict = [NSDictionary dictionaryWithObject:name
                                                       forKey:kURIDictTitleKey];
  return [NSDictionary dictionaryWithObjectsAndKeys:
          kLeafType, kTypeKey,
          nameDict, kURIDictionaryKey,
          urlStr, kURLStringKey,
          nil];
}

// Reads URL(s) off the pasteboard and returns them in BookmarkDictionaryList-
// PboardType format, or nil on failure.
- (NSArray*)readPropertyListFromPasteboard:(NSPasteboard*)pb {
  NSString* type = [pb availableTypeFromArray:
      [outline_ registeredDraggedTypes]];
  if ([type isEqualToString:BookmarkDictionaryListPboardType]) {
    // Safari's full bookmark plist type:
    return [pb propertyListForType:type];

  } else if ([type isEqualToString:WebURLsWithTitlesPboardType]) {
    // Safari's parallel-URLs-and-titles type:
    NSArray* contents = [pb propertyListForType:type];
    NSArray* urlStrings = [contents objectAtIndex:0];
    NSArray* titles = [contents objectAtIndex:1];
    NSUInteger n = [urlStrings count];
    if (n == 0 || [titles count] != n) {
      return nil;
    }
    NSMutableArray* plist = [NSMutableArray array];
    for (NSUInteger i = 0; i < n; i++) {
      [plist addObject:makeBookmarkPlistEntry([titles objectAtIndex:i],
                                              [urlStrings objectAtIndex:i])];
    }
    return plist;

  } else if ([type isEqualToString:NSURLPboardType]) {
    // Standard URL type:
    NSString* urlStr = [[NSURL URLFromPasteboard:pb] absoluteString];
    if (!urlStr) {
      return nil;
    }
    NSString* title = [pb stringForType:@"public.url-name"];
    if (!title)
      title = [pb stringForType:NSStringPboardType];
    return [NSArray arrayWithObject:makeBookmarkPlistEntry(title, urlStr)];

  } else {
    return nil;
  }
}

- (BOOL)isDirectDrag:(id<NSDraggingInfo>)info {
  if (!sDraggedItems)
    return NO;
  if (![[[info draggingPasteboard] types] containsObject:kCustomPboardType])
    return NO;
  id source = [info draggingSource];
  return [source isKindOfClass:[BookmarksOutlineView class]];
}

// Moves BookmarkItems into a parent folder, then selects them.
- (void)moveItems:(NSArray*)items
         toFolder:(BookmarkItem*)dstParent
          atIndex:(int)dstIndex {
  for (BookmarkItem* srcItem in items) {
    // Use an autorelease pool to clean up after the various observers that
    // get called after each individual bookmark change.
    NSAutoreleasePool* pool = [NSAutoreleasePool new];
    BookmarkItem* srcParent = [srcItem parent];
    int srcIndex = [srcParent indexOfChild:srcItem];
    [dstParent moveItem:srcItem toIndex:dstIndex];
    if (srcParent != dstParent || srcIndex >= dstIndex) {
      dstIndex++;
    }
    [pool drain];
  }

  NSRange selRange = {dstIndex - [items count], [items count]};
  [self selectItemsInFolder:dstParent atIndexes:selRange];
}

// Inserts bookmarks in BookmarkDictionaryListPboardType into a folder item.
- (BOOL)insertPropertyList:(NSArray*)plist
                  inFolder:(BookmarkItem*)dstParent
                   atIndex:(NSInteger)dstIndex {
  if (!plist || !dstParent || dstIndex < 0)
    return NO;
  NSInteger i = 0;
  for (NSDictionary* plistItem in plist) {
    // Use an autorelease pool to clean up after the various observers that
    // get called after each individual bookmark change.
    NSAutoreleasePool* pool = [NSAutoreleasePool new];
    if ([[plistItem objectForKey:kTypeKey] isEqual:kLeafType]) {
      NSString* title = [[plistItem objectForKey:kURIDictionaryKey]
                         objectForKey:kURIDictTitleKey];
      NSString* urlStr = [plistItem objectForKey:kURLStringKey];
      if (title && urlStr) {
        [dstParent addBookmarkWithTitle:title
                                    URL:urlStr
                                atIndex:dstIndex + i];
        ++i;
      }
    } else {
      NSString* title = [plistItem objectForKey:kTitleKey];
      NSArray* children = [plistItem objectForKey:kChildrenKey];
      if (title && children) {
        BookmarkItem* newFolder = [dstParent addFolderWithTitle:title
                                                        atIndex:dstIndex + i];
        ++i;
        [self insertPropertyList:children
                        inFolder:newFolder
                         atIndex:0];
      }
    }
    [pool drain];
  }
  [self selectItemsInFolder:dstParent
                  atIndexes:NSMakeRange(dstIndex, [plist count])];
  return YES;
}

// Determine the parent to insert into and the child index to insert at.
- (BookmarkItem*)itemForDropOnItem:(BookmarkItem*)item
                     proposedIndex:(NSInteger*)childIndex {
  BookmarkItem* targetItem = item ? item : group_;
  if ([targetItem isFolder]) {
    if (*childIndex == NSOutlineViewDropOnItemIndex) {
      // Insert it at the end, if we were dropping on it
      *childIndex = [targetItem numberOfChildren];
    }
  } else {
    if (*childIndex == NSOutlineViewDropOnItemIndex) {
      // Can't drop directly on a leaf.
      return nil;
    } else {
      // We will be dropping on the item's parent at the target index
      // of this child, plus one.
      BookmarkItem* oldTargetItem = targetItem;
      targetItem = [targetItem parent];
      *childIndex = [targetItem indexOfChild:oldTargetItem] + 1;
    }
  }
  if ([targetItem isFake]) {
    targetItem = nil;
  }
  return targetItem;
}

// Validates whether or not the proposed drop is valid.
- (NSDragOperation)outlineView:(NSOutlineView*)outlineView
                  validateDrop:(id <NSDraggingInfo>)info
                  proposedItem:(id)item
            proposedChildIndex:(NSInteger)childIndex {
  // Determine the parent to insert into and the child index to insert at.
  BookmarkItem* targetItem = [self itemForDropOnItem:item
                                       proposedIndex:&childIndex];
  if (!targetItem)
    return NSDragOperationNone;

  // Dragging within the outline?
  if ([self isDirectDrag:info]) {
    // If dragging within the tree, we see if we are dragging from ourselves
    // and dropping into a descendant, which wouldn't be allowed...
    for (BookmarkItem* draggedItem in sDraggedItems.get()) {
      if ([draggedItem hasDescendant:targetItem]) {
        return NSDragOperationNone;
      }
    }
    return NSDragOperationMove;
  }

  // Drag from elsewhere is a copy.
  return NSDragOperationCopy;
}

// Actually handles the drop.
- (BOOL)outlineView:(NSOutlineView*)outlineView
         acceptDrop:(id <NSDraggingInfo>)info
               item:(id)item
         childIndex:(NSInteger)childIndex
{
  // Determine the parent to insert into and the child index to insert at.
  BookmarkItem* targetItem = [self itemForDropOnItem:item
                                       proposedIndex:&childIndex];
  if (!targetItem)
    return NO;

  if ([self isDirectDrag:info]) {
    // If the source was ourselves, move the selected items.
    [self moveItems:sDraggedItems
           toFolder:targetItem
            atIndex:childIndex];
    return YES;
  } else {
    // Else copy.
    NSArray* plist = [self readPropertyListFromPasteboard:
        [info draggingPasteboard]];
    return [self insertPropertyList:plist
                           inFolder:targetItem
                            atIndex:childIndex];
  }
}


// The Paste command.
- (IBAction)paste:(id)sender {
  if (![self pasteFromPasteboard:[NSPasteboard generalPasteboard]])
    NSBeep();
}

- (BOOL)pasteFromPasteboard:(NSPasteboard*)pb {
  NSArray* plist = [self readPropertyListFromPasteboard:pb];
  if (!plist)
    return NO;

  BookmarkItem* parentItem;
  NSInteger childIndex;
  int selRow = [outline_ selectedRow];
  if (selRow >= 0) {
    // Insert at selected row.
    BookmarkItem* selItem = [outline_ itemAtRow:selRow];
    parentItem = [outline_ parentForItem:selItem];
    if (!parentItem)
      parentItem = group_;
    childIndex = [parentItem indexOfChild:selItem];
  } else {
    // ...or at very end if there's no selection:
    parentItem = group_;
    childIndex = [parentItem numberOfChildren];
  }
  return [self insertPropertyList:plist
                  inFolder:parentItem
                   atIndex:childIndex];
}

@end
