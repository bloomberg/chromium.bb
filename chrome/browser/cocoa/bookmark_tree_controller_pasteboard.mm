// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/bookmark_tree_controller.h"

#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#import "chrome/browser/cocoa/bookmark_manager_controller.h"
#include "googleurl/src/gurl.h"


// Safari uses this type, though it's not declared in any header.
static NSString* const BookmarkDictionaryListPboardType =
    @"BookmarkDictionaryListPboardType";

// Mac WebKit uses this type, declared in WebKit/mac/History/WebURLsWithTitles.h
static NSString* const WebURLsWithTitlesPboardType =
    @"WebURLsWithTitlesPboardType";

// Used internally to identify intra-outline drags.
static NSString* const kCustomPboardType =
    @"ChromeBookmarkTreeControllerPlaceholderType";


@implementation BookmarkTreeController (Pasteboard)


// One-time dnd setup; called from -awakeFromNib.
- (void)registerDragTypes {
  [outline_ registerForDraggedTypes:[NSArray arrayWithObjects:
                                     BookmarkDictionaryListPboardType,
                                     WebURLsWithTitlesPboardType,
                                     NSURLPboardType, nil]];
  [outline_ setDraggingSourceOperationMask:NSDragOperationEvery forLocal:YES];
  [outline_ setDraggingSourceOperationMask:NSDragOperationEvery forLocal:NO];
}

// Selects a range of items in a parent node.
- (void)selectNodesInFolder:(const BookmarkNode*)parent
                  atIndexes:(NSRange)childRange {
  DCHECK(NSMaxRange(childRange) <= (NSUInteger)parent->GetChildCount());
  id parentItem = [self itemFromNode:parent];
  if (parentItem != nil) {
    // If parent is not the root, need to offset range by parent's index:
    int startRow = [outline_ rowForItem:parentItem];
    if (startRow < 0) {
      return;
    }
    if ([outline_ isItemExpanded:parentItem]) {
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


// Generates parallel arrays of URLs and titles for contents of a node.
static void flattenNode(const BookmarkNode* node,
                        NSMutableArray* urlStrings,
                        NSMutableArray* titles) {
  if (node->is_folder()) {
    for (int i = 0; i < node->GetChildCount(); i++) {
      flattenNode(node->GetChild(i), urlStrings, titles);
    }
  } else if (node->is_url()) {
    [urlStrings addObject:base::SysUTF8ToNSString(
        node->GetURL().possibly_invalid_spec())];
    [titles addObject:base::SysWideToNSString(node->GetTitle())];
  }
}

// Writes data to the pasteboard given a list of row items.
- (BOOL)writeItems:(NSArray*)items
      toPasteboard:(NSPasteboard*)pb
     includeCustom:(BOOL)includeCustom {
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
  for (id item in items) {
    flattenNode([self nodeFromItem:item], urls, titles);
  }
  [pb setPropertyList:[NSArray arrayWithObjects:urls, titles, nil]
              forType:WebURLsWithTitlesPboardType];

  // Add plain text, as one URL per line:
  [pb setString:[urls componentsJoinedByString:@"\n"]
        forType:NSStringPboardType];

  // Add custom type. The actual data doesn't matter since kCustomPboardType
  // drags aren't recognized by anyone but us.
  if (includeCustom) {
    draggedNodes_.clear();
    for (id item in items) {
      draggedNodes_.push_back([self nodeFromItem:item]);
    }
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

// Invoked when dragging outline-view rows.
- (BOOL)outlineView:(NSOutlineView*)outlineView
         writeItems:(NSArray*)items
       toPasteboard:(NSPasteboard*)pb {
  [self writeItems:items toPasteboard:pb includeCustom:YES];
  return YES;
}


// The Cut command.
- (IBAction)cut:(id)sender {
  if ([self writeItems:[self selectedItems]
          toPasteboard:[NSPasteboard generalPasteboard]
         includeCustom:NO]) {
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
  return [self writeItems:[self selectedItems]
             toPasteboard:pb
            includeCustom:NO];
}


#pragma mark -
#pragma mark INCOMING DRAGS AND PASTING:


// BookmarkDictionaryListPboardType represents bookmarks as dictionaries,
// which have the following keys.
// Strangely, folder nodes (whose WebBookmarkType is WebBookmarkTypeLeaf) have
// their title under 'Title', while leaf nodes have it in 'URIDictionary.title'.
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


// Moves BookmarkNodes into a parent folder, then selects them.
- (void)moveNodes:(std::vector<const BookmarkNode*>)nodes
         toFolder:(const BookmarkNode*)dstParent
          atIndex:(int)dstIndex {
  for (std::vector<const BookmarkNode*>::iterator it = nodes.begin();
      it != nodes.end(); ++it) {
    // Use an autorelease pool to clean up after the various observers that
    // get called after each individual bookmark change.
    NSAutoreleasePool* pool = [NSAutoreleasePool new];
    const BookmarkNode* srcNode = *it;
    const BookmarkNode* srcParent = srcNode->GetParent();
    int srcIndex = srcParent->IndexOfChild(srcNode);
    [manager_ bookmarkModel]->Move(srcNode, dstParent, dstIndex);
    if (srcParent != dstParent || srcIndex >= dstIndex) {
      dstIndex++;
    }
    [pool drain];
  }

  [self selectNodesInFolder:dstParent
                  atIndexes:NSMakeRange(dstIndex - nodes.size(), nodes.size())];
}

// Inserts bookmarks in BookmarkDictionaryListPboardType into a folder node.
- (void)insertPropertyList:(NSArray*)plist
                  inFolder:(const BookmarkNode*)dstParent
                   atIndex:(NSInteger)dstIndex {
  BookmarkModel* model = [manager_ bookmarkModel];
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
        model->AddURL(dstParent,
                      dstIndex + i,
                      base::SysNSStringToWide(title),
                      GURL(base::SysNSStringToUTF8(urlStr)));
        ++i;
      }
    } else {
      NSString* title = [plistItem objectForKey:kTitleKey];
      NSArray* children = [plistItem objectForKey:kChildrenKey];
      if (title && children) {
        const BookmarkNode* newFolder;
        newFolder = model->AddGroup(dstParent,
                                    dstIndex + i,
                                    base::SysNSStringToWide(title));
        ++i;
        [self insertPropertyList:children
                        inFolder:newFolder
                         atIndex:0];
      }
    }
    [pool drain];
  }
  [self selectNodesInFolder:dstParent
                  atIndexes:NSMakeRange(dstIndex, [plist count])];
}


// Validates whether or not the proposed drop is valid.
- (NSDragOperation)outlineView:(NSOutlineView*)outlineView
                  validateDrop:(id <NSDraggingInfo>)info
                  proposedItem:(id)item
            proposedChildIndex:(NSInteger)childIndex {
  NSPasteboard* pb = [info draggingPasteboard];

  // Check to see what we are proposed to be dropping on
  const BookmarkNode*targetNode = [self nodeFromItem:item];
  if (!targetNode->is_folder()) {
    // The target node is not a container, but a leaf.
    // Refuse the drop (we may get called again with a between)
    if (childIndex == NSOutlineViewDropOnItemIndex) {
      return NSDragOperationNone;
    }
  }

  // Dragging within the outline?
  if ([info draggingSource] == outlineView &&
      [[pb types] containsObject:kCustomPboardType]) {
    // If we are allowing the drop, we see if we are dragging from ourselves
    // and dropping into a descendent, which wouldn't be allowed...
    // See if the appropriate drag information is available on the pasteboard.
    //TODO(snej): Re-implement this
    /*
    if (targetNode != group_ &&
        [[[info draggingPasteboard] types] containsObject:kCustomPboardType]) {
      for (NSDictionary* draggedNode in draggedNodes_) {
        if ([self treeNode:targetNode isDescendantOfNode:draggedNode]) {
          // Yup, it is, refuse it.
          return NSDragOperationNone;
          break;
        }
      }
      */
    return NSDragOperationMove;
  }

  // Drag from elsewhere is a copy.
  return NSDragOperationCopy;
}

// Determine the parent to insert into and the child index to insert at.
- (const BookmarkNode*)nodeForDropOnItem:(id)item
                           proposedIndex:(NSInteger*)childIndex {
  const BookmarkNode* targetNode = [self nodeFromItem:item];
  if (!targetNode->is_folder()) {
    // If our target is a leaf, and we are dropping on it.
    if (*childIndex == NSOutlineViewDropOnItemIndex) {
      return NULL;
    } else {
      // We will be dropping on the item's parent at the target index
      // of this child, plus one.
      const BookmarkNode* oldTargetNode = targetNode;
      targetNode = targetNode->GetParent();
      *childIndex = targetNode->IndexOfChild(oldTargetNode) + 1;
    }
  } else {
    if (*childIndex == NSOutlineViewDropOnItemIndex) {
      // Insert it at the end, if we were dropping on it
      *childIndex = targetNode->GetChildCount();
    }
  }
  return targetNode;
}

// Actually handles the drop.
- (BOOL)outlineView:(NSOutlineView*)outlineView
         acceptDrop:(id <NSDraggingInfo>)info
               item:(id)item
         childIndex:(NSInteger)childIndex
{
  NSPasteboard* pb = [info draggingPasteboard];

  // Determine the parent to insert into and the child index to insert at.
  const BookmarkNode* targetNode = [self nodeForDropOnItem:item
                                             proposedIndex:&childIndex];
  if (!targetNode)
    return NO;

  if ([info draggingSource] == outlineView &&
      [[pb types] containsObject:kCustomPboardType]) {
    // If the source was ourselves, move the selected nodes.
    [self moveNodes:draggedNodes_
           toFolder:targetNode
            atIndex:childIndex];
  } else {
    NSArray* plist = [self readPropertyListFromPasteboard:pb];
    if (!plist) {
      return NO;
    }
    [self insertPropertyList:plist
                    inFolder:targetNode
                     atIndex:childIndex];
  }
  return YES;
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

  const BookmarkNode* targetNode;
  NSInteger childIndex;
  int selRow = [outline_ selectedRow];
  if (selRow >= 0) {
    // Insert after selected row.
    const BookmarkNode* selNode = [self nodeFromItem:
        [outline_ itemAtRow:selRow]];
    targetNode = selNode->GetParent();
    childIndex = targetNode->IndexOfChild(selNode) + 1;
  } else {
    // ...or at very end if there's no selection:
    targetNode = [self nodeFromItem:group_];
    childIndex = targetNode->GetChildCount();
  }

  [self insertPropertyList:plist
                  inFolder:targetNode
                   atIndex:childIndex];
  return YES;
}

@end
