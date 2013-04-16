// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/hash.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "base/sha1.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "net/disk_cache/simple/simple_index.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const uint64 kTestHashKey = 4364515;
const int64 kTestLastUsedTimeInternal = 12345;
const base::Time kTestLastUsedTime =
    base::Time::FromInternalValue(kTestLastUsedTimeInternal);
const uint64 kTestEntrySize = 789;

}  // namespace

using disk_cache::EntryMetadata;

class EntryMetadataTest  : public testing::Test {
 public:
  EntryMetadata NewEntryMetadataWithValues() {
    return EntryMetadata(kTestHashKey,
                         kTestLastUsedTime,
                         kTestEntrySize);
  }

  void CheckEntryMetadataValues(const EntryMetadata& entry_metadata) {
    EXPECT_EQ(kTestLastUsedTime, entry_metadata.GetLastUsedTime());
    EXPECT_EQ(kTestEntrySize, entry_metadata.GetEntrySize());
  }
};

TEST_F(EntryMetadataTest, Basics) {
  EntryMetadata entry_metadata;
  EXPECT_EQ(base::Time::FromInternalValue(0), entry_metadata.GetLastUsedTime());
  EXPECT_EQ(size_t(0), entry_metadata.GetEntrySize());

  entry_metadata = NewEntryMetadataWithValues();
  CheckEntryMetadataValues(entry_metadata);
  EXPECT_EQ(kTestHashKey, entry_metadata.GetHashKey());

  const base::Time new_time = base::Time::FromInternalValue(5);
  entry_metadata.SetLastUsedTime(new_time);
  EXPECT_EQ(new_time, entry_metadata.GetLastUsedTime());
}

TEST_F(EntryMetadataTest, Serialize) {
  EntryMetadata entry_metadata = NewEntryMetadataWithValues();

  Pickle pickle;
  entry_metadata.Serialize(&pickle);

  PickleIterator it(pickle);
  EntryMetadata new_entry_metadata;
  new_entry_metadata.Deserialize(&it);
  CheckEntryMetadataValues(new_entry_metadata);
  EXPECT_EQ(kTestHashKey, new_entry_metadata.GetHashKey());
}

TEST_F(EntryMetadataTest, Merge) {
  EntryMetadata entry_metadata_a = NewEntryMetadataWithValues();
  // MergeWith assumes the hash_key of both entries is the same, so we
  // initialize it to be that way.
  base::Time dummy_time = base::Time::FromInternalValue(0);
  EntryMetadata entry_metadata_b(entry_metadata_a.GetHashKey(), dummy_time, 0);
  entry_metadata_b.MergeWith(entry_metadata_a);
  CheckEntryMetadataValues(entry_metadata_b);

  EntryMetadata entry_metadata_c(entry_metadata_a.GetHashKey(), dummy_time, 0);
  entry_metadata_a.MergeWith(entry_metadata_c);
  CheckEntryMetadataValues(entry_metadata_a);

  EntryMetadata entry_metadata_d(entry_metadata_a.GetHashKey(), dummy_time, 0);
  const base::Time new_time = base::Time::FromInternalValue(5);
  entry_metadata_d.SetLastUsedTime(new_time);
  entry_metadata_d.MergeWith(entry_metadata_a);
  EXPECT_EQ(entry_metadata_a.GetEntrySize(), entry_metadata_d.GetEntrySize());
  EXPECT_EQ(new_time, entry_metadata_d.GetLastUsedTime());

  EntryMetadata entry_metadata_e(entry_metadata_a.GetHashKey(), dummy_time, 0);
  const uint64 entry_size = 9999999;
  entry_metadata_e.SetEntrySize(entry_size);
  entry_metadata_e.MergeWith(entry_metadata_a);
  EXPECT_EQ(entry_size, entry_metadata_e.GetEntrySize());
  EXPECT_EQ(entry_metadata_a.GetLastUsedTime(),
            entry_metadata_e.GetLastUsedTime());
}
