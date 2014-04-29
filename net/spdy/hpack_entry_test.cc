// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_entry.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

using std::string;

class HpackEntryTest : public ::testing::Test {
 protected:
  HpackEntryTest()
      : name_("header-name"),
        value_("header value"),
        total_insertions_(0),
        table_size_(0) {}

  // These builders maintain the same external table invariants that a "real"
  // table (ie HpackHeaderTable) would.
  HpackEntry StaticEntry() {
    return HpackEntry(name_, value_, true, total_insertions_++, &table_size_);
  }
  HpackEntry DynamicEntry() {
    ++table_size_;
    size_t index = total_insertions_++;
    return HpackEntry(name_, value_, false, index, &total_insertions_);
  }
  void DropEntry() { --table_size_; }

  size_t Size() {
    return name_.size() + value_.size() + HpackEntry::kSizeOverhead;
  }

  string name_, value_;

 private:
  // Referenced by HpackEntry instances.
  size_t total_insertions_;
  size_t table_size_;
};

TEST_F(HpackEntryTest, StaticConstructor) {
  HpackEntry entry(StaticEntry());

  EXPECT_EQ(name_, entry.name());
  EXPECT_EQ(value_, entry.value());
  EXPECT_TRUE(entry.IsStatic());
  EXPECT_EQ(1u, entry.Index());
  EXPECT_EQ(0u, entry.state());
  EXPECT_EQ(Size(), entry.Size());
}

TEST_F(HpackEntryTest, DynamicConstructor) {
  HpackEntry entry(DynamicEntry());

  EXPECT_EQ(name_, entry.name());
  EXPECT_EQ(value_, entry.value());
  EXPECT_FALSE(entry.IsStatic());
  EXPECT_EQ(1u, entry.Index());
  EXPECT_EQ(0u, entry.state());
  EXPECT_EQ(Size(), entry.Size());
}

TEST_F(HpackEntryTest, LookupConstructor) {
  HpackEntry entry(name_, value_);

  EXPECT_EQ(name_, entry.name());
  EXPECT_EQ(value_, entry.value());
  EXPECT_FALSE(entry.IsStatic());
  EXPECT_EQ(0u, entry.Index());
  EXPECT_EQ(0u, entry.state());
  EXPECT_EQ(Size(), entry.Size());
}

TEST_F(HpackEntryTest, DefaultConstructor) {
  HpackEntry entry;

  EXPECT_TRUE(entry.name().empty());
  EXPECT_TRUE(entry.value().empty());
  EXPECT_EQ(0u, entry.state());
  EXPECT_EQ(HpackEntry::kSizeOverhead, entry.Size());
}

TEST_F(HpackEntryTest, IndexUpdate) {
  HpackEntry static1(StaticEntry());
  HpackEntry static2(StaticEntry());

  EXPECT_EQ(1u, static1.Index());
  EXPECT_EQ(2u, static2.Index());

  HpackEntry dynamic1(DynamicEntry());
  HpackEntry dynamic2(DynamicEntry());

  EXPECT_EQ(1u, dynamic2.Index());
  EXPECT_EQ(2u, dynamic1.Index());
  EXPECT_EQ(3u, static1.Index());
  EXPECT_EQ(4u, static2.Index());

  DropEntry();  // Drops |dynamic1|.

  EXPECT_EQ(1u, dynamic2.Index());
  EXPECT_EQ(2u, static1.Index());
  EXPECT_EQ(3u, static2.Index());

  HpackEntry dynamic3(DynamicEntry());

  EXPECT_EQ(1u, dynamic3.Index());
  EXPECT_EQ(2u, dynamic2.Index());
  EXPECT_EQ(3u, static1.Index());
  EXPECT_EQ(4u, static2.Index());
}

TEST_F(HpackEntryTest, ComparatorNameOrdering) {
  HpackEntry entry1(StaticEntry());
  name_[0]--;
  HpackEntry entry2(StaticEntry());

  EXPECT_FALSE(HpackEntry::Comparator()(&entry1, &entry2));
  EXPECT_TRUE(HpackEntry::Comparator()(&entry2, &entry1));
}

TEST_F(HpackEntryTest, ComparatorValueOrdering) {
  HpackEntry entry1(StaticEntry());
  value_[0]--;
  HpackEntry entry2(StaticEntry());

  EXPECT_FALSE(HpackEntry::Comparator()(&entry1, &entry2));
  EXPECT_TRUE(HpackEntry::Comparator()(&entry2, &entry1));
}

TEST_F(HpackEntryTest, ComparatorIndexOrdering) {
  HpackEntry entry1(StaticEntry());
  HpackEntry entry2(StaticEntry());

  EXPECT_TRUE(HpackEntry::Comparator()(&entry1, &entry2));
  EXPECT_FALSE(HpackEntry::Comparator()(&entry2, &entry1));

  HpackEntry entry3(DynamicEntry());
  HpackEntry entry4(DynamicEntry());

  // |entry4| has lower index than |entry3|.
  EXPECT_TRUE(HpackEntry::Comparator()(&entry4, &entry3));
  EXPECT_FALSE(HpackEntry::Comparator()(&entry3, &entry4));

  // |entry3| has lower index than |entry1|.
  EXPECT_TRUE(HpackEntry::Comparator()(&entry3, &entry1));
  EXPECT_FALSE(HpackEntry::Comparator()(&entry1, &entry3));

  // |entry1| & |entry2| ordering is preserved, though each Index() has changed.
  EXPECT_TRUE(HpackEntry::Comparator()(&entry1, &entry2));
  EXPECT_FALSE(HpackEntry::Comparator()(&entry2, &entry1));
}

TEST_F(HpackEntryTest, ComparatorEqualityOrdering) {
  HpackEntry entry1(StaticEntry());
  HpackEntry entry2(DynamicEntry());

  EXPECT_FALSE(HpackEntry::Comparator()(&entry1, &entry1));
  EXPECT_FALSE(HpackEntry::Comparator()(&entry2, &entry2));
}

}  // namespace

}  // namespace net
