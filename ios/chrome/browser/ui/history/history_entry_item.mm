// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/history_entry_item.h"

#include "base/strings/sys_string_conversions.h"
#include "components/history/core/browser/browsing_history_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#pragma mark - HistoryEntryItem

@implementation HistoryEntryItem
@synthesize text = _text;
@synthesize detailText = _detailText;
@synthesize timeText = _timeText;
@synthesize URL = _URL;
@synthesize timestamp = _timestamp;

#pragma mark NSObject

- (BOOL)isEqualToHistoryEntryItem:(HistoryEntryItem*)item {
  return item && item.URL == _URL && item.timestamp == _timestamp;
}

- (BOOL)isEqual:(id)object {
  if (self == object)
    return YES;

  if (![object isMemberOfClass:[HistoryEntryItem class]])
    return NO;

  return [self isEqualToHistoryEntryItem:object];
}

- (NSUInteger)hash {
  return [base::SysUTF8ToNSString(self.URL.spec()) hash] ^
         self.timestamp.since_origin().InMicroseconds();
}

@end
