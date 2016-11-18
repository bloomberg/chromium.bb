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
NSString* const kReadingListEntryTitleKey = @"title";
NSString* const kReadingListEntryURLKey = @"URL";
NSString* const kReadingListEntryStateKey = @"state";
NSString* const kReadingListEntryDistilledPathKey = @"distilledPath";

ReadingListEntry DecodeReadingListEntry(NSData* data) {
  NSError* error = nil;
  NSDictionary* dictionary =
      [NSKeyedUnarchiver unarchiveTopLevelObjectWithData:data error:&error];
  NSString* title = base::mac::ObjCCastStrict<NSString>(
      [dictionary objectForKey:kReadingListEntryTitleKey]);
  NSURL* url = base::mac::ObjCCastStrict<NSURL>(
      [dictionary objectForKey:kReadingListEntryURLKey]);
  auto state(static_cast<ReadingListEntry::DistillationState>(
      [[dictionary objectForKey:kReadingListEntryStateKey] intValue]));
  DCHECK(title && url);
  GURL gurl(net::GURLWithNSURL(url));
  DCHECK(gurl.is_valid());
  ReadingListEntry entry(gurl, base::SysNSStringToUTF8(title));

  switch (state) {
    case ReadingListEntry::PROCESSED: {
      NSString* distilled_path = base::mac::ObjCCastStrict<NSString>(
          [dictionary objectForKey:kReadingListEntryDistilledPathKey]);
      if (distilled_path) {
        base::FilePath path =
            base::FilePath(base::SysNSStringToUTF8(distilled_path));
        entry.SetDistilledPath(path);
      }
      break;
    }
    case ReadingListEntry::PROCESSING:
    case ReadingListEntry::WILL_RETRY:
    case ReadingListEntry::ERROR:
      entry.SetDistilledState(state);
      break;
    case ReadingListEntry::WAITING:
      break;
  }

  return entry;
}

NSData* EncodeReadingListEntry(const ReadingListEntry& entry) {
  NSMutableDictionary* dictionary =
      [NSMutableDictionary dictionaryWithDictionary:@{
        kReadingListEntryTitleKey : base::SysUTF8ToNSString(entry.Title()),
        kReadingListEntryURLKey : net::NSURLWithGURL(entry.URL()),
        kReadingListEntryStateKey :
            [NSNumber numberWithInt:entry.DistilledState()]
      }];

  const base::FilePath path(entry.DistilledPath());
  if (!path.empty()) {
    NSString* distilled_path = base::SysUTF8ToNSString(path.value());
    if (distilled_path)
      [dictionary setObject:distilled_path
                     forKey:kReadingListEntryDistilledPathKey];
  }
  return [NSKeyedArchiver archivedDataWithRootObject:dictionary];
}

}  // namespace

ReadingListModelStorageDefaults::ReadingListModelStorageDefaults() {
  backend_ = [[NSUserDefaults standardUserDefaults] retain];
}

ReadingListModelStorageDefaults::ReadingListModelStorageDefaults(
    NSUserDefaults* backend) {
  DCHECK(backend);
  backend_ = [backend retain];
}

ReadingListModelStorageDefaults::~ReadingListModelStorageDefaults() {
  [backend_ release];
}

std::vector<ReadingListEntry>
ReadingListModelStorageDefaults::LoadPersistentReadList() {
  std::vector<ReadingListEntry> read;
  NSArray* readList = base::mac::ObjCCastStrict<NSArray>(
      [backend_ objectForKey:kReadingListReadElements]);
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
  NSArray* unreadList = base::mac::ObjCCastStrict<NSArray>(
      [backend_ objectForKey:kReadingListUnreadElements]);
  if (unreadList) {
    for (NSData* entryData : unreadList) {
      unread.push_back(DecodeReadingListEntry(entryData));
    }
  }
  return unread;
}

void ReadingListModelStorageDefaults::SavePersistentReadList(
    const std::vector<ReadingListEntry>& read) {
  NSMutableArray* read_list = [NSMutableArray arrayWithCapacity:read.size()];
  for (const ReadingListEntry& entry : read) {
    [read_list addObject:EncodeReadingListEntry(entry)];
  }
  [backend_ setObject:read_list forKey:kReadingListReadElements];
}

void ReadingListModelStorageDefaults::SavePersistentUnreadList(
    const std::vector<ReadingListEntry>& unread) {
  NSMutableArray* unread_list =
      [NSMutableArray arrayWithCapacity:unread.size()];
  for (const ReadingListEntry& entry : unread) {
    [unread_list addObject:EncodeReadingListEntry(entry)];
  }
  [backend_ setObject:unread_list forKey:kReadingListUnreadElements];
}

