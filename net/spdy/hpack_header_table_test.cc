// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/hpack_header_table.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/macros.h"
#include "net/spdy/hpack_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

using std::string;

typedef std::vector<HpackEntry> HpackEntryVector;

// Returns an entry whose Size() is equal to the given one.
HpackEntry MakeEntryOfSize(uint32 size) {
  EXPECT_GE(size, HpackEntry::kSizeOverhead);
  string name((size - HpackEntry::kSizeOverhead) / 2, 'n');
  string value(size - HpackEntry::kSizeOverhead - name.size(), 'v');
  HpackEntry entry(name, value);
  EXPECT_EQ(size, entry.Size());
  return entry;
}

// Returns a vector of entries whose total size is equal to the given
// one.
HpackEntryVector MakeEntriesOfTotalSize(uint32 total_size) {
  EXPECT_GE(total_size, HpackEntry::kSizeOverhead);
  uint32 entry_size = HpackEntry::kSizeOverhead;
  uint32 remaining_size = total_size;
  HpackEntryVector entries;
  while (remaining_size > 0) {
    EXPECT_LE(entry_size, remaining_size);
    entries.push_back(MakeEntryOfSize(entry_size));
    remaining_size -= entry_size;
    entry_size = std::min(remaining_size, entry_size + 32);
  }
  return entries;
}

// Adds the given vector of entries to the given header table,
// expecting no eviction to happen.
void AddEntriesExpectNoEviction(const HpackEntryVector& entries,
                                HpackHeaderTable* header_table) {
  unsigned start_entry_count = header_table->GetEntryCount();
  for (HpackEntryVector::const_iterator it = entries.begin();
       it != entries.end(); ++it) {
    uint32 index = 0;
    std::vector<uint32> removed_referenced_indices;
    header_table->TryAddEntry(*it, &index, &removed_referenced_indices);
    EXPECT_EQ(1u, index);
    EXPECT_TRUE(removed_referenced_indices.empty());
    EXPECT_EQ(start_entry_count + (it - entries.begin()) + 1u,
              header_table->GetEntryCount());
  }

  for (HpackEntryVector::const_iterator it = entries.begin();
       it != entries.end(); ++it) {
    uint32 index = header_table->GetEntryCount() - (it - entries.begin());
    HpackEntry entry = header_table->GetEntry(index);
    EXPECT_TRUE(it->Equals(entry))
        << "it = " << it->GetDebugString() << " != entry = "
        << entry.GetDebugString();
  }
}

// Returns the set of all indices in header_table that are in that
// table's reference set.
std::set<uint32> GetReferenceSet(const HpackHeaderTable& header_table) {
  std::set<uint32> reference_set;
  for (uint32 i = 1; i <= header_table.GetEntryCount(); ++i) {
    if (header_table.GetEntry(i).IsReferenced()) {
      reference_set.insert(i);
    }
  }
  return reference_set;
}

// Fill a header table with entries. Make sure the entries are in
// reverse order in the header table.
TEST(HpackHeaderTableTest, TryAddEntryBasic) {
  HpackHeaderTable header_table;
  EXPECT_EQ(0u, header_table.size());

  HpackEntryVector entries = MakeEntriesOfTotalSize(header_table.max_size());

  // Most of the checks are in AddEntriesExpectNoEviction().
  AddEntriesExpectNoEviction(entries, &header_table);
  EXPECT_EQ(header_table.max_size(), header_table.size());
}

// Fill a header table with entries, and then ramp the table's max
// size down to evict an entry one at a time. Make sure the eviction
// happens as expected.
TEST(HpackHeaderTableTest, SetMaxSize) {
  HpackHeaderTable header_table;

  HpackEntryVector entries = MakeEntriesOfTotalSize(header_table.max_size());
  AddEntriesExpectNoEviction(entries, &header_table);

  for (HpackEntryVector::const_iterator it = entries.begin();
       it != entries.end(); ++it) {
    uint32 expected_count = entries.end() - it;
    EXPECT_EQ(expected_count, header_table.GetEntryCount());

    header_table.SetMaxSize(header_table.size() + 1);
    EXPECT_EQ(expected_count, header_table.GetEntryCount());

    header_table.SetMaxSize(header_table.size());
    EXPECT_EQ(expected_count, header_table.GetEntryCount());

    --expected_count;
    header_table.SetMaxSize(header_table.size() - 1);
    EXPECT_EQ(expected_count, header_table.GetEntryCount());
  }

  EXPECT_EQ(0u, header_table.size());
}

// Setting the max size of a header table to zero should clear its
// reference set.
TEST(HpackHeaderTableTest, SetMaxSizeZeroClearsReferenceSet) {
  HpackHeaderTable header_table;

  HpackEntryVector entries = MakeEntriesOfTotalSize(header_table.max_size());
  AddEntriesExpectNoEviction(entries, &header_table);

  std::set<uint32> expected_reference_set;
  for (uint32 i = 1; i <= header_table.GetEntryCount(); ++i) {
    header_table.GetMutableEntry(i)->SetReferenced(true);
    expected_reference_set.insert(i);
  }
  EXPECT_EQ(expected_reference_set, GetReferenceSet(header_table));

  header_table.SetMaxSize(0);
  EXPECT_TRUE(GetReferenceSet(header_table).empty());
}

// Fill a header table with entries, and then add an entry just big
// enough to cause eviction of all but one entry. Make sure the
// eviction happens as expected and the long entry is inserted into
// the table.
TEST(HpackHeaderTableTest, TryAddEntryEviction) {
  HpackHeaderTable header_table;

  HpackEntryVector entries = MakeEntriesOfTotalSize(header_table.max_size());
  AddEntriesExpectNoEviction(entries, &header_table);

  EXPECT_EQ(entries.size(), header_table.GetEntryCount());
  HpackEntry first_entry = header_table.GetEntry(1);
  HpackEntry long_entry =
      MakeEntryOfSize(header_table.size() - first_entry.Size());

  header_table.SetMaxSize(header_table.size());
  EXPECT_EQ(entries.size(), header_table.GetEntryCount());

  std::set<uint32> expected_reference_set;
  for (uint32 i = 2; i <= header_table.GetEntryCount(); ++i) {
    header_table.GetMutableEntry(i)->SetReferenced(true);
    expected_reference_set.insert(i);
  }
  EXPECT_EQ(expected_reference_set, GetReferenceSet(header_table));

  uint32 index = 0;
  std::vector<uint32> removed_referenced_indices;
  header_table.TryAddEntry(long_entry, &index, &removed_referenced_indices);

  EXPECT_EQ(1u, index);
  EXPECT_EQ(expected_reference_set,
            std::set<uint32>(removed_referenced_indices.begin(),
                             removed_referenced_indices.end()));
  EXPECT_TRUE(GetReferenceSet(header_table).empty());
  EXPECT_EQ(2u, header_table.GetEntryCount());
  EXPECT_TRUE(header_table.GetEntry(1).Equals(long_entry));
  EXPECT_TRUE(header_table.GetEntry(2).Equals(first_entry));
}

// Fill a header table with entries, and then add an entry bigger than
// the entire table. Make sure no entry remains in the table.
TEST(HpackHeaderTableTest, TryAddTooLargeEntry) {
  HpackHeaderTable header_table;

  HpackEntryVector entries = MakeEntriesOfTotalSize(header_table.max_size());
  AddEntriesExpectNoEviction(entries, &header_table);

  header_table.SetMaxSize(header_table.size());
  EXPECT_EQ(entries.size(), header_table.GetEntryCount());

  std::set<uint32> expected_removed_referenced_indices;
  for (uint32 i = 1; i <= header_table.GetEntryCount(); ++i) {
    header_table.GetMutableEntry(i)->SetReferenced(true);
    expected_removed_referenced_indices.insert(i);
  }

  HpackEntry long_entry = MakeEntryOfSize(header_table.size() + 1);
  uint32 index = 0;
  std::vector<uint32> removed_referenced_indices;
  header_table.TryAddEntry(long_entry, &index, &removed_referenced_indices);

  EXPECT_EQ(0u, index);
  EXPECT_EQ(expected_removed_referenced_indices,
            std::set<uint32>(removed_referenced_indices.begin(),
                             removed_referenced_indices.end()));
  EXPECT_EQ(0u, header_table.GetEntryCount());
}

}  // namespace

}  // namespace net
