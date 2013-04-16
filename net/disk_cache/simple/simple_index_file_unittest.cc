// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/hash.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "net/disk_cache/simple/simple_entry_format.h"
#include "net/disk_cache/simple/simple_index.h"
#include "net/disk_cache/simple/simple_index_file.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using disk_cache::SimpleIndexFile;
using disk_cache::SimpleIndex;

namespace disk_cache {

class IndexMetadataTest : public testing::Test {};

TEST_F(IndexMetadataTest, Basics) {
  SimpleIndexFile::IndexMetadata index_metadata;

  EXPECT_EQ(disk_cache::kSimpleIndexMagicNumber, index_metadata.magic_number_);
  EXPECT_EQ(disk_cache::kSimpleVersion, index_metadata.version_);
  EXPECT_EQ(0U, index_metadata.GetNumberOfEntries());
  EXPECT_EQ(0U, index_metadata.cache_size_);

  EXPECT_TRUE(index_metadata.CheckIndexMetadata());
}

TEST_F(IndexMetadataTest, Serialize) {
  SimpleIndexFile::IndexMetadata index_metadata(123, 456);
  Pickle pickle;
  index_metadata.Serialize(&pickle);
  PickleIterator it(pickle);
  SimpleIndexFile::IndexMetadata new_index_metadata;
  new_index_metadata.Deserialize(&it);

  EXPECT_EQ(new_index_metadata.magic_number_, index_metadata.magic_number_);
  EXPECT_EQ(new_index_metadata.version_, index_metadata.version_);
  EXPECT_EQ(new_index_metadata.GetNumberOfEntries(),
            index_metadata.GetNumberOfEntries());
  EXPECT_EQ(new_index_metadata.cache_size_, index_metadata.cache_size_);

  EXPECT_TRUE(new_index_metadata.CheckIndexMetadata());
}

class SimpleIndexFileTest : public testing::Test {
 public:
  bool CompareTwoEntryMetadata(const EntryMetadata& a, const EntryMetadata& b) {
    return a.hash_key_ == b.hash_key_ &&
        a.last_used_time_ == b.last_used_time_ &&
        a.entry_size_ == b.entry_size_;
  }

};

TEST_F(SimpleIndexFileTest, Serialize) {
  SimpleIndex::EntrySet entries;
  EntryMetadata entries_array[] = {
    EntryMetadata(11, Time::FromInternalValue(22), 33),
    EntryMetadata(22, Time::FromInternalValue(33), 44),
    EntryMetadata(33, Time::FromInternalValue(44), 55)
  };
  SimpleIndexFile::IndexMetadata index_metadata(arraysize(entries_array), 456);
  for (uint32 i = 0; i < arraysize(entries_array); ++i) {
    SimpleIndex::InsertInEntrySet(entries_array[i], &entries);
  }

  scoped_ptr<Pickle> pickle = SimpleIndexFile::Serialize(
      index_metadata, entries);
  EXPECT_TRUE(pickle.get() != NULL);

  scoped_ptr<SimpleIndex::EntrySet> new_entries = SimpleIndexFile::Deserialize(
      reinterpret_cast<const char*>(pickle->data()),
      pickle->size());
  EXPECT_TRUE(new_entries.get() != NULL);
  EXPECT_EQ(entries.size(), new_entries->size());

  for (uint32 i = 0; i < arraysize(entries_array); ++i) {
    SimpleIndex::EntrySet::iterator it =
        new_entries->find(entries_array[i].GetHashKey());
    EXPECT_TRUE(new_entries->end() != it);
    EXPECT_TRUE(CompareTwoEntryMetadata(it->second, entries_array[i]));
  }
}

}
