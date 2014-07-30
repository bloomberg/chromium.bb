// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_pasteboard_helper_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/files/file_path.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "ui/base/clipboard/clipboard.h"

using bookmarks::BookmarkNodeData;

NSString* const kBookmarkDictionaryListPboardType =
    @"BookmarkDictionaryListPboardType";

namespace {

// An unofficial standard pasteboard title type to be provided alongside the
// |NSURLPboardType|.
NSString* const kNSURLTitlePboardType = @"public.url-name";

// Pasteboard type used to store profile path to determine which profile
// a set of bookmarks came from.
NSString* const kChromiumProfilePathPboardType =
    @"ChromiumProfilePathPboardType";

// Internal bookmark ID for a bookmark node.  Used only when moving inside
// of one profile.
NSString* const kChromiumBookmarkId = @"ChromiumBookmarkId";

// Mac WebKit uses this type, declared in
// WebKit/mac/History/WebURLsWithTitles.h.
NSString* const kCrWebURLsWithTitlesPboardType = @"WebURLsWithTitlesPboardType";

// Keys for the type of node in BookmarkDictionaryListPboardType.
NSString* const kWebBookmarkType = @"WebBookmarkType";

NSString* const kWebBookmarkTypeList = @"WebBookmarkTypeList";

NSString* const kWebBookmarkTypeLeaf = @"WebBookmarkTypeLeaf";

void ConvertPlistToElements(NSArray* input,
                            std::vector<BookmarkNodeData::Element>& elements) {
  NSUInteger len = [input count];
  for (NSUInteger i = 0; i < len; ++i) {
    NSDictionary* pboardBookmark = [input objectAtIndex:i];
    scoped_ptr<BookmarkNode> new_node(new BookmarkNode(GURL()));
    int64 node_id =
        [[pboardBookmark objectForKey:kChromiumBookmarkId] longLongValue];
    new_node->set_id(node_id);
    BOOL is_folder = [[pboardBookmark objectForKey:kWebBookmarkType]
        isEqualToString:kWebBookmarkTypeList];
    if (is_folder) {
      new_node->set_type(BookmarkNode::FOLDER);
      NSString* title = [pboardBookmark objectForKey:@"Title"];
      new_node->SetTitle(base::SysNSStringToUTF16(title));
    } else {
      new_node->set_type(BookmarkNode::URL);
      NSDictionary* uriDictionary =
          [pboardBookmark objectForKey:@"URIDictionary"];
      NSString* title = [uriDictionary objectForKey:@"title"];
      NSString* urlString = [pboardBookmark objectForKey:@"URLString"];
      new_node->SetTitle(base::SysNSStringToUTF16(title));
      new_node->set_url(GURL(base::SysNSStringToUTF8(urlString)));
    }
    BookmarkNodeData::Element e = BookmarkNodeData::Element(new_node.get());
    if(is_folder)
      ConvertPlistToElements([pboardBookmark objectForKey:@"Children"],
                             e.children);
    elements.push_back(e);
  }
}

bool ReadBookmarkDictionaryListPboardType(
    NSPasteboard* pb,
    std::vector<BookmarkNodeData::Element>& elements) {
  NSArray* bookmarks =
      [pb propertyListForType:kBookmarkDictionaryListPboardType];
  if (!bookmarks)
    return false;
  ConvertPlistToElements(bookmarks, elements);
  return true;
}

bool ReadWebURLsWithTitlesPboardType(
    NSPasteboard* pb,
    std::vector<BookmarkNodeData::Element>& elements) {
  NSArray* bookmarkPairs =
      [pb propertyListForType:kCrWebURLsWithTitlesPboardType];
  if (![bookmarkPairs isKindOfClass:[NSArray class]])
    return false;

  NSArray* urlsArr = [bookmarkPairs objectAtIndex:0];
  NSArray* titlesArr = [bookmarkPairs objectAtIndex:1];
  if ([urlsArr count] < 1)
    return false;
  if ([urlsArr count] != [titlesArr count])
    return false;

  NSUInteger len = [titlesArr count];
  for (NSUInteger i = 0; i < len; ++i) {
    base::string16 title =
        base::SysNSStringToUTF16([titlesArr objectAtIndex:i]);
    std::string url = base::SysNSStringToUTF8([urlsArr objectAtIndex:i]);
    if (!url.empty()) {
      BookmarkNodeData::Element element;
      element.is_url = true;
      element.url = GURL(url);
      element.title = title;
      elements.push_back(element);
    }
  }
  return true;
}

bool ReadNSURLPboardType(NSPasteboard* pb,
                         std::vector<BookmarkNodeData::Element>& elements) {
  NSURL* url = [NSURL URLFromPasteboard:pb];
  if (url == nil)
    return false;

  std::string urlString = base::SysNSStringToUTF8([url absoluteString]);
  NSString* title = [pb stringForType:kNSURLTitlePboardType];
  if (!title)
    title = [pb stringForType:NSStringPboardType];

  BookmarkNodeData::Element element;
  element.is_url = true;
  element.url = GURL(urlString);
  element.title = base::SysNSStringToUTF16(title);
  elements.push_back(element);
  return true;
}

NSArray* GetPlistForBookmarkList(
    const std::vector<BookmarkNodeData::Element>& elements) {
  NSMutableArray* plist = [NSMutableArray array];
  for (size_t i = 0; i < elements.size(); ++i) {
    BookmarkNodeData::Element element = elements[i];
    if (element.is_url) {
      NSString* title = base::SysUTF16ToNSString(element.title);
      NSString* url = base::SysUTF8ToNSString(element.url.spec());
      int64 elementId = element.id();
      NSNumber* idNum = [NSNumber numberWithLongLong:elementId];
      NSDictionary* uriDictionary = [NSDictionary dictionaryWithObjectsAndKeys:
              title, @"title", nil];
      NSDictionary* object = [NSDictionary dictionaryWithObjectsAndKeys:
          uriDictionary, @"URIDictionary",
          url, @"URLString",
          kWebBookmarkTypeLeaf, kWebBookmarkType,
          idNum, kChromiumBookmarkId,
          nil];
      [plist addObject:object];
    } else {
      NSString* title = base::SysUTF16ToNSString(element.title);
      NSArray* children = GetPlistForBookmarkList(element.children);
      int64 elementId = element.id();
      NSNumber* idNum = [NSNumber numberWithLongLong:elementId];
      NSDictionary* object = [NSDictionary dictionaryWithObjectsAndKeys:
          title, @"Title",
          children, @"Children",
          kWebBookmarkTypeList, kWebBookmarkType,
          idNum, kChromiumBookmarkId,
          nil];
      [plist addObject:object];
    }
  }
  return plist;
}

void WriteBookmarkDictionaryListPboardType(
    NSPasteboard* pb,
    const std::vector<BookmarkNodeData::Element>& elements) {
  NSArray* plist = GetPlistForBookmarkList(elements);
  [pb setPropertyList:plist forType:kBookmarkDictionaryListPboardType];
}

void FillFlattenedArraysForBookmarks(
    const std::vector<BookmarkNodeData::Element>& elements,
    NSMutableArray* titles,
    NSMutableArray* urls) {
  for (size_t i = 0; i < elements.size(); ++i) {
    BookmarkNodeData::Element element = elements[i];
    if (element.is_url) {
      NSString* title = base::SysUTF16ToNSString(element.title);
      NSString* url = base::SysUTF8ToNSString(element.url.spec());
      [titles addObject:title];
      [urls addObject:url];
    } else {
      FillFlattenedArraysForBookmarks(element.children, titles, urls);
    }
  }
}

void WriteSimplifiedBookmarkTypes(
    NSPasteboard* pb,
    const std::vector<BookmarkNodeData::Element>& elements) {
  NSMutableArray* titles = [NSMutableArray array];
  NSMutableArray* urls = [NSMutableArray array];
  FillFlattenedArraysForBookmarks(elements, titles, urls);

  // These bookmark types only act on urls, not folders.
  if ([urls count] < 1)
    return;

  // Write WebURLsWithTitlesPboardType.
  [pb setPropertyList:[NSArray arrayWithObjects:urls, titles, nil]
              forType:kCrWebURLsWithTitlesPboardType];

  // Write NSStringPboardType.
  [pb setString:[urls componentsJoinedByString:@"\n"]
        forType:NSStringPboardType];

  // Write NSURLPboardType (with title).
  NSURL* url = [NSURL URLWithString:[urls objectAtIndex:0]];
  [url writeToPasteboard:pb];
  NSString* titleString = [titles objectAtIndex:0];
  [pb setString:titleString forType:kNSURLTitlePboardType];
}

NSPasteboard* PasteboardFromType(ui::ClipboardType type) {
  NSString* type_string = nil;
  switch (type) {
    case ui::CLIPBOARD_TYPE_COPY_PASTE:
      type_string = NSGeneralPboard;
      break;
    case ui::CLIPBOARD_TYPE_DRAG:
      type_string = NSDragPboard;
      break;
    case ui::CLIPBOARD_TYPE_SELECTION:
      NOTREACHED();
      break;
  }

  return [NSPasteboard pasteboardWithName:type_string];
}

}  // namespace

void WriteBookmarksToPasteboard(
    ui::ClipboardType type,
    const std::vector<BookmarkNodeData::Element>& elements,
    const base::FilePath& profile_path) {
  if (elements.empty())
    return;

  NSPasteboard* pb = PasteboardFromType(type);

  NSArray* types = [NSArray arrayWithObjects:kBookmarkDictionaryListPboardType,
                                             kCrWebURLsWithTitlesPboardType,
                                             NSStringPboardType,
                                             NSURLPboardType,
                                             kNSURLTitlePboardType,
                                             kChromiumProfilePathPboardType,
                                             nil];
  [pb declareTypes:types owner:nil];
  [pb setString:base::SysUTF8ToNSString(profile_path.value())
        forType:kChromiumProfilePathPboardType];
  WriteBookmarkDictionaryListPboardType(pb, elements);
  WriteSimplifiedBookmarkTypes(pb, elements);
}

bool ReadBookmarksFromPasteboard(
    ui::ClipboardType type,
    std::vector<BookmarkNodeData::Element>& elements,
    base::FilePath* profile_path) {
  NSPasteboard* pb = PasteboardFromType(type);

  elements.clear();
  NSString* profile = [pb stringForType:kChromiumProfilePathPboardType];
  *profile_path = base::FilePath(base::SysNSStringToUTF8(profile));
  return ReadBookmarkDictionaryListPboardType(pb, elements) ||
         ReadWebURLsWithTitlesPboardType(pb, elements) ||
         ReadNSURLPboardType(pb, elements);
}

bool PasteboardContainsBookmarks(ui::ClipboardType type) {
  NSPasteboard* pb = PasteboardFromType(type);

  NSArray* availableTypes =
      [NSArray arrayWithObjects:kBookmarkDictionaryListPboardType,
                                kCrWebURLsWithTitlesPboardType,
                                NSURLPboardType,
                                nil];
  return [pb availableTypeFromArray:availableTypes] != nil;
}
