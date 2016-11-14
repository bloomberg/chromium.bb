// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "base/mac/scoped_nsobject.h"
#include "ios/chrome/browser/reading_list/reading_list_model_storage_defaults.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
NSUserDefaults* FakeNonPersistentUserDefaults() {
  // Storage only uses two methods on NSUserDefaults: -setObject:forKey: and
  // -objectForKey:, a dictionary is a perfect replacement for it. If it looks
  // like a duck, swims like a duck and quacks as a duck, then it probably is a
  // duck.
  return (NSUserDefaults*)[[[NSMutableDictionary alloc] init] autorelease];
}
}

TEST(ReadingListStorage, CheckEmpties) {
  ReadingListModelStorageDefaults storage(FakeNonPersistentUserDefaults());

  std::vector<ReadingListEntry> entries;

  entries = storage.LoadPersistentReadList();
  EXPECT_EQ(0ul, entries.size());
  entries = storage.LoadPersistentUnreadList();
  EXPECT_EQ(0ul, entries.size());
}

TEST(ReadingListStorage, SaveOneRead) {
  NSUserDefaults* defaults = FakeNonPersistentUserDefaults();
  std::vector<ReadingListEntry> entries;
  entries.push_back(
      ReadingListEntry(GURL("http://read.example.com"), "read title"));

  {
    ReadingListModelStorageDefaults storage(defaults);
    storage.SavePersistentReadList(entries);
  }

  ReadingListModelStorageDefaults storage(defaults);

  const std::vector<ReadingListEntry> restored_entries(
      storage.LoadPersistentReadList());
  EXPECT_EQ(1ul, restored_entries.size());
  EXPECT_EQ(GURL("http://read.example.com"), restored_entries[0].URL());
  EXPECT_EQ("read title", restored_entries[0].Title());
  EXPECT_EQ("read title", restored_entries[0].Title());
}

TEST(ReadingListStorage, SaveOneUnead) {
  NSUserDefaults* defaults = FakeNonPersistentUserDefaults();
  std::vector<ReadingListEntry> entries;
  entries.push_back(
      ReadingListEntry(GURL("http://unread.example.com"), "unread title"));

  {
    ReadingListModelStorageDefaults storage(defaults);
    storage.SavePersistentUnreadList(entries);
  }

  ReadingListModelStorageDefaults storage(defaults);
  const std::vector<ReadingListEntry> restored_entries(
      storage.LoadPersistentUnreadList());
  EXPECT_EQ(1ul, restored_entries.size());
  EXPECT_EQ(GURL("http://unread.example.com"), restored_entries[0].URL());
  EXPECT_EQ("unread title", restored_entries[0].Title());
}

TEST(ReadingListStorage, SaveOneModified) {
  NSUserDefaults* defaults = FakeNonPersistentUserDefaults();
  std::vector<ReadingListEntry> entries;
  entries.push_back(
      ReadingListEntry(GURL("http://unread.example.com"), "unread title"));
  {
    ReadingListModelStorageDefaults storage(defaults);
    entries[0].SetDistilledPath(base::FilePath("distilled/page.html"));
    storage.SavePersistentUnreadList(entries);
  }

  ReadingListModelStorageDefaults storage(defaults);
  const std::vector<ReadingListEntry> restored_entries(
      storage.LoadPersistentUnreadList());
  EXPECT_EQ(1ul, restored_entries.size());
  EXPECT_EQ(GURL("http://unread.example.com"), restored_entries[0].URL());
  EXPECT_EQ(base::FilePath("distilled/page.html"),
            restored_entries[0].DistilledPath());
  EXPECT_EQ(GURL("chrome://offline/distilled/page.html"),
            restored_entries[0].DistilledURL());
  EXPECT_EQ("unread title", restored_entries[0].Title());
}
