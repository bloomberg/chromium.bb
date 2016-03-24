// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "ios/chrome/browser/reading_list/reading_list_model_storage_defaults.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "net/base/mac/url_conversions.h"

namespace {

NSString* const kReadingListReadElements = @"ReadingListReadElements";
NSString* const kReadingListUnreadElements = @"ReadingListUnreadElements";
NSString* const kReadingListUnseenState = @"ReadingListUnseenState";
NSString* const kReadingListEntryTitleKey = @"title";
NSString* const kReadingListEntryURLKey = @"URL";

ReadingListEntry DecodeReadingListEntry(NSData* data) {
  NSError* error = nil;
  NSDictionary* dictionary =
      [NSKeyedUnarchiver unarchiveTopLevelObjectWithData:data error:&error];
  NSString* title = base::mac::ObjCCastStrict<NSString>(
      [dictionary objectForKey:kReadingListEntryTitleKey]);
  NSURL* url = base::mac::ObjCCastStrict<NSURL>(
      [dictionary objectForKey:kReadingListEntryURLKey]);
  DCHECK(title && url);
  GURL gurl(net::GURLWithNSURL(url));
  DCHECK(gurl.is_valid());
  return ReadingListEntry(gurl, base::SysNSStringToUTF8(title));
}

NSData* EncodeReadingListEntry(const ReadingListEntry& entry) {
  NSDictionary* dictionary = @{
    kReadingListEntryTitleKey : base::SysUTF8ToNSString(entry.title()),
    kReadingListEntryURLKey : net::NSURLWithGURL(entry.url())
  };
  return [NSKeyedArchiver archivedDataWithRootObject:dictionary];
}

}  // namespace

std::vector<ReadingListEntry>
ReadingListModelStorageDefaults::LoadPersistentReadList() {
  std::vector<ReadingListEntry> read;
  NSArray* readList =
      base::mac::ObjCCastStrict<NSArray>([[NSUserDefaults standardUserDefaults]
          objectForKey:kReadingListReadElements]);
  if (readList) {
    for (NSData* entryData : readList) {
      read.push_back(DecodeReadingListEntry(entryData));
    }
  }
  return read;
}

std::vector<ReadingListEntry>
ReadingListModelStorageDefaults::LoadPersistentUnreadList() {
  std::vector<ReadingListEntry> unread;
  NSArray* unreadList =
      base::mac::ObjCCastStrict<NSArray>([[NSUserDefaults standardUserDefaults]
          objectForKey:kReadingListUnreadElements]);
  if (unreadList) {
    for (NSData* entryData : unreadList) {
      unread.push_back(DecodeReadingListEntry(entryData));
    }
  }
  return unread;
}

bool ReadingListModelStorageDefaults::LoadPersistentHasUnseen() {
  return [[[NSUserDefaults standardUserDefaults]
      objectForKey:kReadingListUnseenState] boolValue];
}

void ReadingListModelStorageDefaults::SavePersistentReadList(
    const std::vector<ReadingListEntry>& read) {
  NSMutableArray* read_list = [NSMutableArray arrayWithCapacity:read.size()];
  for (const ReadingListEntry& entry : read) {
    [read_list addObject:EncodeReadingListEntry(entry)];
  }
  [[NSUserDefaults standardUserDefaults] setObject:read_list
                                            forKey:kReadingListReadElements];
}

void ReadingListModelStorageDefaults::SavePersistentUnreadList(
    const std::vector<ReadingListEntry>& unread) {
  NSMutableArray* unread_list =
      [NSMutableArray arrayWithCapacity:unread.size()];
  for (const ReadingListEntry& entry : unread) {
    [unread_list addObject:EncodeReadingListEntry(entry)];
  }
  [[NSUserDefaults standardUserDefaults] setValue:unread_list
                                           forKey:kReadingListUnreadElements];
}

void ReadingListModelStorageDefaults::SavePersistentHasUnseen(bool has_unseen) {
  [[NSUserDefaults standardUserDefaults]
      setObject:[NSNumber numberWithBool:has_unseen]
         forKey:kReadingListUnseenState];
}
