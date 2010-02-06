// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/sys_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_pasteboard_helper_mac.h"

namespace bookmark_pasteboard_helper_mac {

// Mac WebKit uses this type, declared in
// WebKit/mac/History/WebURLsWithTitles.h
static NSString* const WebURLsWithTitlesPboardType =
    @"WebURLsWithTitlesPboardType";

static NSString* const BookmarkDictionaryListPboardType =
    @"BookmarkDictionaryListPboardType";

// Keys for the type of node in BookmarkDictionaryListPboardType
static NSString* const WebBookmarkType =
    @"WebBookmarkType";

static NSString* const WebBookmarkTypeList =
    @"WebBookmarkTypeList";

static NSString* const WebBookmarkTypeLeaf =
    @"WebBookmarkTypeLeaf";

static void ConvertPlistToElements(NSArray* input,
                            std::vector<BookmarkDragData::Element>& elements) {
  NSUInteger len = [input count];
  for (NSUInteger i = 0; i < len; ++i) {
    NSDictionary* object = [input objectAtIndex:i];
    BookmarkDragData::Element element;
    BOOL is_folder = [[object objectForKey:WebBookmarkType]
        isEqualToString:WebBookmarkTypeList];
    if(is_folder) {
      NSString* title = [object objectForKey:@"Title"];
      element.title = base::SysNSStringToUTF16(title);
      ConvertPlistToElements([object objectForKey:@"Children"],
                             element.children);
    } else {
      NSDictionary* uriDictionary = [object objectForKey:@"URIDictionary"];
      NSString* title = [uriDictionary objectForKey:@"title"];
      NSString* urlString = [object objectForKey:@"URLString"];
      element.title = base::SysNSStringToUTF16(title);
      element.url = GURL(base::SysNSStringToUTF8(urlString));
      element.is_url = true;
    }
    elements.push_back(element);
  }
}

static bool ReadBookmarkDictionaryListPboardType(NSPasteboard* pb,
      std::vector<BookmarkDragData::Element>& elements) {
  NSArray* bookmarks = [pb propertyListForType:
      BookmarkDictionaryListPboardType];
  if (!bookmarks) return false;
  ConvertPlistToElements(bookmarks, elements);
  return true;
}

static bool ReadWebURLsWithTitlesPboardType(NSPasteboard* pb,
      std::vector<BookmarkDragData::Element>& elements) {
  NSArray* bookmarkPairs =
      [pb propertyListForType:WebURLsWithTitlesPboardType];
  if (![bookmarkPairs isKindOfClass:[NSArray class]]) {
    return false;
  }
  NSArray* urlsArr = [bookmarkPairs objectAtIndex:0];
  NSArray* titlesArr = [bookmarkPairs objectAtIndex:1];
  if ([urlsArr count] < 1) {
    return false;
  }
  if ([urlsArr count] != [titlesArr count]) {
    return false;
  }

  int len = [titlesArr count];
  for (int i = 0; i < len; ++i) {
    string16 title = base::SysNSStringToUTF16([titlesArr objectAtIndex:i]);
    std::string url = base::SysNSStringToUTF8([urlsArr objectAtIndex:i]);
    if (!url.empty()) {
      BookmarkDragData::Element element;
      element.is_url = true;
      element.url = GURL(url);
      element.title = title;
      elements.push_back(element);
    }
  }
  return true;
}

static bool ReadNSURLPboardType(NSPasteboard* pb,
                         std::vector<BookmarkDragData::Element>& elements) {
  NSURL* url = [NSURL URLFromPasteboard:pb];
  if (url == nil) {
    return false;
  }
  std::string urlString = base::SysNSStringToUTF8([url absoluteString]);
  NSString* title = [pb stringForType:@"public.url-name"];
  if (!title)
    title = [pb stringForType:NSStringPboardType];

  BookmarkDragData::Element element;
  element.is_url = true;
  element.url = GURL(urlString);
  element.title = base::SysNSStringToUTF16(title);
  elements.push_back(element);
  return true;
}

static NSArray* GetPlistForBookmarkList(
    const std::vector<BookmarkDragData::Element>& elements) {
  NSMutableArray* plist = [NSMutableArray array];
  for (size_t i = 0; i < elements.size(); ++i) {
    BookmarkDragData::Element element = elements[i];
    if (element.is_url) {
      NSString* title = base::SysUTF16ToNSString(element.title);
      NSString* url = base::SysUTF8ToNSString(element.url.spec());
      NSDictionary* uriDictionary = [NSDictionary dictionaryWithObjectsAndKeys:
              title, @"title", nil];
      NSDictionary* object = [NSDictionary dictionaryWithObjectsAndKeys:
          uriDictionary, @"URIDictionary",
          url, @"URLString",
          WebBookmarkTypeLeaf, WebBookmarkType,
          nil];
      [plist addObject:object];
    } else {
      NSString* title = base::SysUTF16ToNSString(element.title);
      NSArray* children = GetPlistForBookmarkList(element.children);
      NSDictionary* object = [NSDictionary dictionaryWithObjectsAndKeys:
          title, @"Title",
          children, @"Children",
          WebBookmarkTypeList, WebBookmarkType,
          nil];
      [plist addObject:object];
    }
  }
  return plist;
}

static void WriteBookmarkDictionaryListPboardType(NSPasteboard* pb,
    const std::vector<BookmarkDragData::Element>& elements) {
  NSArray* plist = GetPlistForBookmarkList(elements);
  [pb setPropertyList:plist forType:BookmarkDictionaryListPboardType];
}

static void FillFlattenedArraysForBookmarks(
    const std::vector<BookmarkDragData::Element>& elements,
    NSMutableArray* titles, NSMutableArray* urls) {
  for (size_t i = 0; i < elements.size(); ++i) {
    BookmarkDragData::Element element = elements[i];
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

static void WriteSimplifiedBookmarkTypes(NSPasteboard* pb,
    const std::vector<BookmarkDragData::Element>& elements) {
  NSMutableArray* titles = [NSMutableArray array];
  NSMutableArray* urls = [NSMutableArray array];
  FillFlattenedArraysForBookmarks(elements, titles, urls);

  //Write WebURLsWithTitlesPboardType
  [pb setPropertyList:[NSArray arrayWithObjects:urls, titles, nil]
              forType:WebURLsWithTitlesPboardType];

  //Write NSStringPboardType
  [pb setString:[urls componentsJoinedByString:@"\n"]
      forType:NSStringPboardType];

  // Write NSURLPboardType
  NSURL* url = [NSURL URLWithString:[urls objectAtIndex:0]];
  [url writeToPasteboard:pb];
  NSString* titleString = [titles objectAtIndex:0];
  [pb setString:titleString forType:@"public.url-name"];
}

void WriteToClipboard(const std::vector<BookmarkDragData::Element>& elements) {
  if (elements.size() == 0) {
    return;
  }
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  NSArray* types = [NSArray arrayWithObjects:BookmarkDictionaryListPboardType,
                                             WebURLsWithTitlesPboardType,
                                             NSStringPboardType,
                                             NSURLPboardType,
                                             @"public.url-name",
                                             nil];
  [pb declareTypes:types owner:nil];
  WriteBookmarkDictionaryListPboardType(pb, elements);
  WriteSimplifiedBookmarkTypes(pb, elements);
}

bool ReadFromClipboard(std::vector<BookmarkDragData::Element>& elements) {
  elements.clear();

  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  return (ReadBookmarkDictionaryListPboardType(pb, elements) ||
         ReadWebURLsWithTitlesPboardType(pb, elements) ||
         ReadNSURLPboardType(pb, elements));
}

bool ClipboardContainsBookmarks() {
  NSPasteboard* pb = [NSPasteboard generalPasteboard];
  NSArray* availableTypes = [NSArray arrayWithObjects:
                                BookmarkDictionaryListPboardType,
                                WebURLsWithTitlesPboardType,
                                NSURLPboardType,
                                nil];
  return [pb availableTypeFromArray:availableTypes] != nil;
}

}
