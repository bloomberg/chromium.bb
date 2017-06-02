// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/history_entry_inserter.h"

#import "base/mac/foundation_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/history/history_entry.h"
#import "ios/chrome/browser/ui/history/history_entry_item.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

HistoryEntryItem* TestHistoryEntryItem(base::Time timestamp,
                                       const std::string& name) {
  history::HistoryEntry entry = history::HistoryEntry(
      history::HistoryEntry::LOCAL_ENTRY, GURL(("http://" + name).c_str()),
      base::UTF8ToUTF16(name.c_str()), timestamp, std::string(), false,
      base::string16(), false);
  return [[HistoryEntryItem alloc] initWithType:kItemTypeEnumZero
                                   historyEntry:entry
                                   browserState:nil
                                       delegate:nil];
}

// Test fixture for HistoryEntryInserter.
class HistoryEntryInserterTest : public PlatformTest {
 public:
  HistoryEntryInserterTest() {
    model_ = [[CollectionViewModel alloc] init];
    [model_ addSectionWithIdentifier:kSectionIdentifierEnumZero];
    inserter_ = [[HistoryEntryInserter alloc] initWithModel:model_];
    mock_delegate_ =
        [OCMockObject mockForProtocol:@protocol(HistoryEntryInserterDelegate)];
    [inserter_ setDelegate:mock_delegate_];
  }

 protected:
  __strong CollectionViewModel* model_;
  __strong HistoryEntryInserter* inserter_;
  __strong id<HistoryEntryInserterDelegate> mock_delegate_;
};

// Tests that history entry items added to CollectionViewModel are sorted by
// timestamp.
TEST_F(HistoryEntryInserterTest, AddItems) {
  base::Time today =
      base::Time::Now().LocalMidnight() + base::TimeDelta::FromHours(1);
  base::TimeDelta minute = base::TimeDelta::FromMinutes(1);
  HistoryEntryItem* entry1 = TestHistoryEntryItem(today, "entry1");
  HistoryEntryItem* entry2 = TestHistoryEntryItem(today - minute, "entry2");
  HistoryEntryItem* entry3 =
      TestHistoryEntryItem(today - 2 * (minute), "entry3");

  OCMockObject* mock_delegate = (OCMockObject*)mock_delegate_;
  [[mock_delegate expect] historyEntryInserter:inserter_
                       didInsertSectionAtIndex:1];
  [[mock_delegate expect]
          historyEntryInserter:inserter_
      didInsertItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:1]];
  [inserter_ insertHistoryEntryItem:entry2];
  EXPECT_OCMOCK_VERIFY(mock_delegate);

  [[mock_delegate expect]
          historyEntryInserter:inserter_
      didInsertItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:1]];
  [inserter_ insertHistoryEntryItem:entry1];
  EXPECT_OCMOCK_VERIFY(mock_delegate);

  [[mock_delegate expect]
          historyEntryInserter:inserter_
      didInsertItemAtIndexPath:[NSIndexPath indexPathForItem:2 inSection:1]];
  [inserter_ insertHistoryEntryItem:entry3];
  EXPECT_OCMOCK_VERIFY(mock_delegate);

  EXPECT_EQ(2, [model_ numberOfSections]);
  EXPECT_EQ(0, [model_ numberOfItemsInSection:0]);
  EXPECT_EQ(3, [model_ numberOfItemsInSection:1]);

  NSArray<HistoryEntryItem*>* section_1 =
      base::mac::ObjCCastStrict<NSArray<HistoryEntryItem*>>(
          [model_ itemsInSectionWithIdentifier:kSectionIdentifierEnumZero + 1]);
  EXPECT_NSEQ(@"entry1", section_1[0].text);
  EXPECT_NSEQ(@"entry2", section_1[1].text);
  EXPECT_NSEQ(@"entry3", section_1[2].text);
  EXPECT_OCMOCK_VERIFY(mock_delegate);
}

// Tests that items from different dates are added in correctly ordered
// sections.
TEST_F(HistoryEntryInserterTest, AddSections) {
  base::Time today =
      base::Time::Now().LocalMidnight() + base::TimeDelta::FromHours(12);
  base::TimeDelta day = base::TimeDelta::FromDays(1);
  base::TimeDelta minute = base::TimeDelta::FromMinutes(1);
  HistoryEntryItem* day1 = TestHistoryEntryItem(today, "day1");
  HistoryEntryItem* day2_entry1 =
      TestHistoryEntryItem(today - day, "day2_entry1");
  HistoryEntryItem* day2_entry2 =
      TestHistoryEntryItem(today - day - minute, "day2_entry2");
  HistoryEntryItem* day3 = TestHistoryEntryItem(today - 2 * day, "day3");

  OCMockObject* mock_delegate = (OCMockObject*)mock_delegate_;

  [[mock_delegate expect] historyEntryInserter:inserter_
                       didInsertSectionAtIndex:1];
  [[mock_delegate expect]
          historyEntryInserter:inserter_
      didInsertItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:1]];
  [inserter_ insertHistoryEntryItem:day2_entry2];
  NSInteger day2_identifier = kSectionIdentifierEnumZero + 1;
  EXPECT_EQ(2, [model_ numberOfSections]);
  EXPECT_EQ(0, [model_ numberOfItemsInSection:0]);
  EXPECT_EQ(1, [model_ numberOfItemsInSection:1]);
  NSArray<HistoryEntryItem*>* section_1 =
      base::mac::ObjCCastStrict<NSArray<HistoryEntryItem*>>(
          [model_ itemsInSectionWithIdentifier:day2_identifier]);
  EXPECT_NSEQ(@"day2_entry2", section_1[0].text);
  EXPECT_OCMOCK_VERIFY(mock_delegate);

  [[mock_delegate expect] historyEntryInserter:inserter_
                       didInsertSectionAtIndex:1];
  [[mock_delegate expect]
          historyEntryInserter:inserter_
      didInsertItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:1]];
  [inserter_ insertHistoryEntryItem:day1];
  NSInteger day1_identifier = kSectionIdentifierEnumZero + 2;
  EXPECT_EQ(3, [model_ numberOfSections]);
  EXPECT_EQ(0, [model_ numberOfItemsInSection:0]);
  EXPECT_EQ(1, [model_ numberOfItemsInSection:1]);
  EXPECT_EQ(1, [model_ numberOfItemsInSection:2]);
  section_1 = base::mac::ObjCCastStrict<NSArray<HistoryEntryItem*>>(
      [model_ itemsInSectionWithIdentifier:day1_identifier]);
  EXPECT_NSEQ(@"day1", section_1[0].text);
  NSArray<HistoryEntryItem*>* section_2 =
      base::mac::ObjCCastStrict<NSArray<HistoryEntryItem*>>(
          [model_ itemsInSectionWithIdentifier:day2_identifier]);
  EXPECT_NSEQ(@"day2_entry2", section_2[0].text);
  EXPECT_OCMOCK_VERIFY(mock_delegate);

  [[mock_delegate expect] historyEntryInserter:inserter_
                       didInsertSectionAtIndex:3];
  [[mock_delegate expect]
          historyEntryInserter:inserter_
      didInsertItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:3]];
  [inserter_ insertHistoryEntryItem:day3];
  NSInteger day3_identifier = kSectionIdentifierEnumZero + 3;
  EXPECT_EQ(4, [model_ numberOfSections]);
  EXPECT_EQ(0, [model_ numberOfItemsInSection:0]);
  EXPECT_EQ(1, [model_ numberOfItemsInSection:1]);
  EXPECT_EQ(1, [model_ numberOfItemsInSection:2]);
  EXPECT_EQ(1, [model_ numberOfItemsInSection:3]);
  section_1 = base::mac::ObjCCastStrict<NSArray<HistoryEntryItem*>>(
      [model_ itemsInSectionWithIdentifier:day1_identifier]);
  EXPECT_NSEQ(@"day1", section_1[0].text);
  section_2 = base::mac::ObjCCastStrict<NSArray<HistoryEntryItem*>>(
      [model_ itemsInSectionWithIdentifier:day2_identifier]);
  EXPECT_NSEQ(@"day2_entry2", section_2[0].text);
  NSArray<HistoryEntryItem*>* section_3 =
      base::mac::ObjCCastStrict<NSArray<HistoryEntryItem*>>(
          [model_ itemsInSectionWithIdentifier:day3_identifier]);
  EXPECT_NSEQ(@"day3", section_3[0].text);
  EXPECT_OCMOCK_VERIFY(mock_delegate);

  [[mock_delegate expect]
          historyEntryInserter:inserter_
      didInsertItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:2]];
  [inserter_ insertHistoryEntryItem:day2_entry1];
  EXPECT_EQ(4, [model_ numberOfSections]);
  EXPECT_EQ(0, [model_ numberOfItemsInSection:0]);
  EXPECT_EQ(1, [model_ numberOfItemsInSection:1]);
  EXPECT_EQ(2, [model_ numberOfItemsInSection:2]);
  EXPECT_EQ(1, [model_ numberOfItemsInSection:3]);
  section_1 = base::mac::ObjCCastStrict<NSArray<HistoryEntryItem*>>(
      [model_ itemsInSectionWithIdentifier:day1_identifier]);
  EXPECT_NSEQ(@"day1", section_1[0].text);
  section_2 = base::mac::ObjCCastStrict<NSArray<HistoryEntryItem*>>(
      [model_ itemsInSectionWithIdentifier:day2_identifier]);
  EXPECT_NSEQ(@"day2_entry1", section_2[0].text);
  EXPECT_NSEQ(@"day2_entry2", section_2[1].text);
  section_3 = base::mac::ObjCCastStrict<NSArray<HistoryEntryItem*>>(
      [model_ itemsInSectionWithIdentifier:day3_identifier]);
  EXPECT_NSEQ(@"day3", section_3[0].text);
  EXPECT_OCMOCK_VERIFY(mock_delegate);
}

// Tests that items are only ever added once.
TEST_F(HistoryEntryInserterTest, AddDuplicateItems) {
  base::Time today = base::Time::Now();
  HistoryEntryItem* entry1 = TestHistoryEntryItem(today, "entry");
  HistoryEntryItem* entry2 = TestHistoryEntryItem(today, "entry");

  OCMockObject* mock_delegate = (OCMockObject*)mock_delegate_;
  [[mock_delegate expect] historyEntryInserter:inserter_
                       didInsertSectionAtIndex:1];
  [[mock_delegate expect]
          historyEntryInserter:inserter_
      didInsertItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:1]];
  [inserter_ insertHistoryEntryItem:entry1];
  [inserter_ insertHistoryEntryItem:entry2];

  EXPECT_EQ(2, [model_ numberOfSections]);
  EXPECT_EQ(0, [model_ numberOfItemsInSection:0]);
  EXPECT_EQ(1, [model_ numberOfItemsInSection:1]);

  NSArray<HistoryEntryItem*>* section_1 =
      base::mac::ObjCCastStrict<NSArray<HistoryEntryItem*>>(
          [model_ itemsInSectionWithIdentifier:kSectionIdentifierEnumZero + 1]);
  EXPECT_NSEQ(@"entry", section_1[0].text);
  EXPECT_OCMOCK_VERIFY(mock_delegate);
}

// Tests that removing a section invokes the appropriate delegate callback.
TEST_F(HistoryEntryInserterTest, RemoveSection) {
  base::Time today =
      base::Time::Now().LocalMidnight() + base::TimeDelta::FromHours(1);
  base::TimeDelta day = base::TimeDelta::FromDays(1);
  HistoryEntryItem* day1 = TestHistoryEntryItem(today, "day1");
  HistoryEntryItem* day2 = TestHistoryEntryItem(today - day, "day2");

  OCMockObject* mock_delegate = (OCMockObject*)mock_delegate_;

  [[mock_delegate expect] historyEntryInserter:inserter_
                       didInsertSectionAtIndex:1];
  [[mock_delegate expect]
          historyEntryInserter:inserter_
      didInsertItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:1]];
  [inserter_ insertHistoryEntryItem:day1];
  NSInteger day1_identifier = kSectionIdentifierEnumZero + 1;
  EXPECT_OCMOCK_VERIFY(mock_delegate);

  [[mock_delegate expect] historyEntryInserter:inserter_
                       didInsertSectionAtIndex:2];
  [[mock_delegate expect]
          historyEntryInserter:inserter_
      didInsertItemAtIndexPath:[NSIndexPath indexPathForItem:0 inSection:2]];
  [inserter_ insertHistoryEntryItem:day2];
  EXPECT_EQ(3, [model_ numberOfSections]);
  EXPECT_OCMOCK_VERIFY(mock_delegate);

  // Empty the section for day 1, and remove the section.
  [model_ removeItemWithType:kItemTypeEnumZero
      fromSectionWithIdentifier:day1_identifier];
  [[mock_delegate expect] historyEntryInserter:inserter_
                       didRemoveSectionAtIndex:1];
  [inserter_ removeSection:1];

  EXPECT_EQ(2, [model_ numberOfSections]);
  EXPECT_OCMOCK_VERIFY(mock_delegate);
}
