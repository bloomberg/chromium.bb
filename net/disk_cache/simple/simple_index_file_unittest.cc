// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "net/disk_cache/simple/simple_entry_format.h"
#include "net/disk_cache/simple/simple_index.h"
#include "net/disk_cache/simple/simple_index_file.h"
#include "net/disk_cache/simple/simple_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using disk_cache::SimpleIndexFile;
using disk_cache::SimpleIndex;

namespace disk_cache {

TEST(IndexMetadataTest, Basics) {
  SimpleIndexFile::IndexMetadata index_metadata;

  EXPECT_EQ(disk_cache::kSimpleIndexMagicNumber, index_metadata.magic_number_);
  EXPECT_EQ(disk_cache::kSimpleVersion, index_metadata.version_);
  EXPECT_EQ(0U, index_metadata.GetNumberOfEntries());
  EXPECT_EQ(0U, index_metadata.cache_size_);

  EXPECT_TRUE(index_metadata.CheckIndexMetadata());
}

TEST(IndexMetadataTest, Serialize) {
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

// This friend derived class is able to reexport its ancestors private methods
// as public, for use in tests.
class WrappedSimpleIndexFile : public SimpleIndexFile {
 public:
  using SimpleIndexFile::Deserialize;
  using SimpleIndexFile::IsIndexFileStale;
  using SimpleIndexFile::Serialize;

  explicit WrappedSimpleIndexFile(const base::FilePath& index_file_directory)
      : SimpleIndexFile(base::MessageLoopProxy::current().get(),
                        base::MessageLoopProxy::current().get(),
                        index_file_directory) {}
  virtual ~WrappedSimpleIndexFile() {
  }

  const base::FilePath& GetIndexFilePath() const {
    return index_file_;
  }
};

class SimpleIndexFileTest : public testing::Test {
 public:
  bool CompareTwoEntryMetadata(const EntryMetadata& a, const EntryMetadata& b) {
    return a.last_used_time_ == b.last_used_time_ &&
           a.entry_size_ == b.entry_size_;
  }

 protected:
  SimpleIndexFileTest() : callback_called_(false) {}

  base::Closure GetCallback() {
    return base::Bind(&SimpleIndexFileTest::LoadIndexEntriesCallback,
                      base::Unretained(this));
  }

  bool callback_called() { return callback_called_; }

 private:
  void LoadIndexEntriesCallback() {
    EXPECT_FALSE(callback_called_);
    callback_called_ = true;
  }

  bool callback_called_;
};

TEST_F(SimpleIndexFileTest, Serialize) {
  SimpleIndex::EntrySet entries;
  static const uint64 kHashes[] = { 11, 22, 33 };
  static const size_t kNumHashes = arraysize(kHashes);
  EntryMetadata metadata_entries[kNumHashes];

  SimpleIndexFile::IndexMetadata index_metadata(static_cast<uint64>(kNumHashes),
                                                456);
  for (size_t i = 0; i < kNumHashes; ++i) {
    uint64 hash = kHashes[i];
    metadata_entries[i] =
        EntryMetadata(Time::FromInternalValue(hash), hash);
    SimpleIndex::InsertInEntrySet(hash, metadata_entries[i], &entries);
  }

  scoped_ptr<Pickle> pickle = WrappedSimpleIndexFile::Serialize(
      index_metadata, entries);
  EXPECT_TRUE(pickle.get() != NULL);

  SimpleIndexLoadResult deserialize_result;
  WrappedSimpleIndexFile::Deserialize(static_cast<const char*>(pickle->data()),
                                     pickle->size(),
                                     &deserialize_result);
  EXPECT_TRUE(deserialize_result.did_load);
  const SimpleIndex::EntrySet& new_entries = deserialize_result.entries;
  EXPECT_EQ(entries.size(), new_entries.size());

  for (size_t i = 0; i < kNumHashes; ++i) {
    SimpleIndex::EntrySet::const_iterator it = new_entries.find(kHashes[i]);
    EXPECT_TRUE(new_entries.end() != it);
    EXPECT_TRUE(CompareTwoEntryMetadata(it->second, metadata_entries[i]));
  }
}

TEST_F(SimpleIndexFileTest, IsIndexFileStale) {
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());
  base::Time cache_mtime;
  const base::FilePath cache_path = cache_dir.path();

  ASSERT_TRUE(simple_util::GetMTime(cache_path, &cache_mtime));
  WrappedSimpleIndexFile simple_index_file(cache_path);
  const base::FilePath& index_path = simple_index_file.GetIndexFilePath();
  EXPECT_TRUE(WrappedSimpleIndexFile::IsIndexFileStale(cache_mtime,
                                                       index_path));
  const std::string kDummyData = "nothing to be seen here";
  EXPECT_EQ(static_cast<int>(kDummyData.size()),
            file_util::WriteFile(index_path,
                                 kDummyData.data(),
                                 kDummyData.size()));
  ASSERT_TRUE(simple_util::GetMTime(cache_path, &cache_mtime));
  EXPECT_FALSE(WrappedSimpleIndexFile::IsIndexFileStale(cache_mtime,
                                                        index_path));

  const base::Time past_time = base::Time::Now() -
      base::TimeDelta::FromSeconds(10);
  EXPECT_TRUE(file_util::TouchFile(index_path, past_time, past_time));
  EXPECT_TRUE(file_util::TouchFile(cache_path, past_time, past_time));
  ASSERT_TRUE(simple_util::GetMTime(cache_path, &cache_mtime));
  EXPECT_FALSE(WrappedSimpleIndexFile::IsIndexFileStale(cache_mtime,
                                                        index_path));
  const base::Time even_older =
      past_time - base::TimeDelta::FromSeconds(10);
  EXPECT_TRUE(file_util::TouchFile(index_path, even_older, even_older));
  EXPECT_TRUE(WrappedSimpleIndexFile::IsIndexFileStale(cache_mtime,
                                                       index_path));

}

TEST_F(SimpleIndexFileTest, WriteThenLoadIndex) {
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());

  SimpleIndex::EntrySet entries;
  static const uint64 kHashes[] = { 11, 22, 33 };
  static const size_t kNumHashes = arraysize(kHashes);
  EntryMetadata metadata_entries[kNumHashes];
  for (size_t i = 0; i < kNumHashes; ++i) {
    uint64 hash = kHashes[i];
    metadata_entries[i] =
        EntryMetadata(Time::FromInternalValue(hash), hash);
    SimpleIndex::InsertInEntrySet(hash, metadata_entries[i], &entries);
  }

  const uint64 kCacheSize = 456U;
  {
    WrappedSimpleIndexFile simple_index_file(cache_dir.path());
    simple_index_file.WriteToDisk(entries, kCacheSize,
                                  base::TimeTicks(), false);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(base::PathExists(simple_index_file.GetIndexFilePath()));
  }

  WrappedSimpleIndexFile simple_index_file(cache_dir.path());
  base::Time fake_cache_mtime;
  ASSERT_TRUE(simple_util::GetMTime(simple_index_file.GetIndexFilePath(),
                                    &fake_cache_mtime));
  SimpleIndexLoadResult load_index_result;
  simple_index_file.LoadIndexEntries(fake_cache_mtime,
                                     GetCallback(),
                                     &load_index_result);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(base::PathExists(simple_index_file.GetIndexFilePath()));
  ASSERT_TRUE(callback_called());
  EXPECT_TRUE(load_index_result.did_load);
  EXPECT_FALSE(load_index_result.flush_required);

  EXPECT_EQ(kNumHashes, load_index_result.entries.size());
  for (size_t i = 0; i < kNumHashes; ++i)
    EXPECT_EQ(1U, load_index_result.entries.count(kHashes[i]));
}

TEST_F(SimpleIndexFileTest, LoadCorruptIndex) {
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());

  WrappedSimpleIndexFile simple_index_file(cache_dir.path());
  const base::FilePath& index_path = simple_index_file.GetIndexFilePath();
  const std::string kDummyData = "nothing to be seen here";
  EXPECT_EQ(static_cast<int>(kDummyData.size()),
            file_util::WriteFile(index_path,
                                 kDummyData.data(),
                                 kDummyData.size()));
  base::Time fake_cache_mtime;
  ASSERT_TRUE(simple_util::GetMTime(simple_index_file.GetIndexFilePath(),
                                    &fake_cache_mtime));
  EXPECT_FALSE(WrappedSimpleIndexFile::IsIndexFileStale(fake_cache_mtime,
                                                        index_path));

  SimpleIndexLoadResult load_index_result;
  simple_index_file.LoadIndexEntries(fake_cache_mtime,
                                     GetCallback(),
                                     &load_index_result);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(base::PathExists(index_path));
  ASSERT_TRUE(callback_called());
  EXPECT_TRUE(load_index_result.did_load);
  EXPECT_TRUE(load_index_result.flush_required);
}

}  // namespace disk_cache
