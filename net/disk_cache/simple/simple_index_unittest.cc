// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "base/sha1.h"
#include "base/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "net/disk_cache/simple/simple_index.h"
#include "net/disk_cache/simple/simple_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const uint64 kTestHashKey = 4364515;
const int64 kTestLastUsedTimeInternal = 12345;
const base::Time kTestLastUsedTime =
    base::Time::FromInternalValue(kTestLastUsedTimeInternal);
const uint64 kTestEntrySize = 789;

}  // namespace

namespace disk_cache {

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

class SimpleIndexTest  : public testing::Test {
};

TEST_F(SimpleIndexTest, IsIndexFileStale) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const std::string kIndexFileName = "simple-index";
  const base::FilePath index_path =
      temp_dir.path().AppendASCII(kIndexFileName);
  EXPECT_TRUE(SimpleIndex::IsIndexFileStale(index_path));
  const std::string kDummyData = "nothing to be seen here";
  EXPECT_EQ(static_cast<int>(kDummyData.size()),
            file_util::WriteFile(index_path,
                                 kDummyData.data(),
                                 kDummyData.size()));
  EXPECT_FALSE(SimpleIndex::IsIndexFileStale(index_path));

  const base::Time past_time = base::Time::Now() -
      base::TimeDelta::FromSeconds(10);
  EXPECT_TRUE(file_util::TouchFile(index_path, past_time, past_time));
  EXPECT_TRUE(file_util::TouchFile(temp_dir.path(), past_time, past_time));
  EXPECT_FALSE(SimpleIndex::IsIndexFileStale(index_path));

  EXPECT_EQ(static_cast<int>(kDummyData.size()),
            file_util::WriteFile(temp_dir.path().AppendASCII("other_file"),
                                 kDummyData.data(),
                                 kDummyData.size()));

  EXPECT_TRUE(SimpleIndex::IsIndexFileStale(index_path));
}

TEST_F(SimpleIndexTest, IndexSizeCorrectOnMerge) {
  typedef disk_cache::SimpleIndex::EntrySet EntrySet;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  disk_cache::SimpleIndex index(NULL, NULL, temp_dir.path());
  index.SetMaxSize(100);
  index.Insert("two");
  index.UpdateEntrySize("two", 2);
  index.Insert("five");
  index.UpdateEntrySize("five", 5);
  index.Insert("seven");
  index.UpdateEntrySize("seven", 7);
  EXPECT_EQ(14U, index.cache_size_);
  {
    scoped_ptr<EntrySet> entries(new EntrySet());
    index.MergeInitializingSet(entries.Pass(), false);
  }
  EXPECT_EQ(14U, index.cache_size_);
  {
    scoped_ptr<EntrySet> entries(new EntrySet());
    const uint64 hash_key = simple_util::GetEntryHashKey("eleven");
    entries->insert(std::make_pair(hash_key, EntryMetadata(hash_key,
                                                           base::Time::Now(),
                                                           11)));
    index.MergeInitializingSet(entries.Pass(), false);
  }
  EXPECT_EQ(25U, index.cache_size_);
}

}  // namespace disk_cache
