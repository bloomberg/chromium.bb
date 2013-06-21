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
#include "base/time.h"
#include "net/disk_cache/simple/simple_entry_format.h"
#include "net/disk_cache/simple/simple_index.h"
#include "net/disk_cache/simple/simple_index_file.h"
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

class SimpleIndexFileTest : public testing::Test {
 public:
  bool CompareTwoEntryMetadata(const EntryMetadata& a, const EntryMetadata& b) {
    return a.last_used_time_ == b.last_used_time_ &&
           a.entry_size_ == b.entry_size_;
  }

  void IndexCompletionCallback(
      scoped_ptr<SimpleIndex::EntrySet> index_file_entries,
      bool force_index_flush) {
    EXPECT_FALSE(callback_result_);
    callback_result_.reset(
        new IndexCompletionCallbackResult(index_file_entries.Pass(),
                                          force_index_flush));
  }

 protected:
  struct IndexCompletionCallbackResult {
    IndexCompletionCallbackResult(
        scoped_ptr<SimpleIndex::EntrySet> index_file_entries,
        bool force_index_flush)
        : index_file_entries(index_file_entries.Pass()),
          force_index_flush(force_index_flush) {
    }

    const scoped_ptr<SimpleIndex::EntrySet> index_file_entries;
    const bool force_index_flush;
  };

  IndexCompletionCallbackResult* callback_result() {
    return callback_result_.get();
  }

 private:
  scoped_ptr<IndexCompletionCallbackResult> callback_result_;
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

  scoped_ptr<Pickle> pickle = SimpleIndexFile::Serialize(
      index_metadata, entries);
  EXPECT_TRUE(pickle.get() != NULL);

  scoped_ptr<SimpleIndex::EntrySet> new_entries = SimpleIndexFile::Deserialize(
      reinterpret_cast<const char*>(pickle->data()),
      pickle->size());
  EXPECT_TRUE(new_entries.get() != NULL);
  EXPECT_EQ(entries.size(), new_entries->size());

  for (size_t i = 0; i < kNumHashes; ++i) {
    SimpleIndex::EntrySet::iterator it = new_entries->find(kHashes[i]);
    EXPECT_TRUE(new_entries->end() != it);
    EXPECT_TRUE(CompareTwoEntryMetadata(it->second, metadata_entries[i]));
  }
}

TEST_F(SimpleIndexFileTest, IsIndexFileStale) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const std::string kIndexFileName = "simple-index";
  const base::FilePath index_path =
      temp_dir.path().AppendASCII(kIndexFileName);
  EXPECT_TRUE(SimpleIndexFile::IsIndexFileStale(index_path));
  const std::string kDummyData = "nothing to be seen here";
  EXPECT_EQ(static_cast<int>(kDummyData.size()),
            file_util::WriteFile(index_path,
                                 kDummyData.data(),
                                 kDummyData.size()));
  EXPECT_FALSE(SimpleIndexFile::IsIndexFileStale(index_path));

  const base::Time past_time = base::Time::Now() -
      base::TimeDelta::FromSeconds(10);
  EXPECT_TRUE(file_util::TouchFile(index_path, past_time, past_time));
  EXPECT_TRUE(file_util::TouchFile(temp_dir.path(), past_time, past_time));
  EXPECT_FALSE(SimpleIndexFile::IsIndexFileStale(index_path));

  EXPECT_EQ(static_cast<int>(kDummyData.size()),
            file_util::WriteFile(temp_dir.path().AppendASCII("other_file"),
                                 kDummyData.data(),
                                 kDummyData.size()));

  EXPECT_TRUE(SimpleIndexFile::IsIndexFileStale(index_path));
}

TEST_F(SimpleIndexFileTest, WriteThenLoadIndex) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const base::FilePath index_path =
      temp_dir.path().AppendASCII(SimpleIndexFile::kIndexFileName);
  EXPECT_TRUE(SimpleIndexFile::IsIndexFileStale(index_path));

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
    SimpleIndexFile simple_index_file(base::MessageLoopProxy::current(),
                                      base::MessageLoopProxy::current(),
                                      temp_dir.path());
    simple_index_file.WriteToDisk(entries, kCacheSize,
                                  base::TimeTicks(), false);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(file_util::PathExists(index_path));
  }

  SimpleIndexFile simple_index_file(base::MessageLoopProxy::current(),
                                    base::MessageLoopProxy::current(),
                                    temp_dir.path());
  SimpleIndexFile::IndexCompletionCallback callback =
      base::Bind(&SimpleIndexFileTest::IndexCompletionCallback,
                 base::Unretained(this));
  simple_index_file.LoadIndexEntries(base::MessageLoopProxy::current(),
                                     callback);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(file_util::PathExists(index_path));
  ASSERT_TRUE(callback_result());
  EXPECT_FALSE(callback_result()->force_index_flush);
  const SimpleIndex::EntrySet* read_entries =
      callback_result()->index_file_entries.get();
  ASSERT_TRUE(read_entries);

  EXPECT_EQ(kNumHashes, read_entries->size());
  for (size_t i = 0; i < kNumHashes; ++i)
    EXPECT_EQ(1U, read_entries->count(kHashes[i]));
}

TEST_F(SimpleIndexFileTest, IsIndexFileCorrupt) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const base::FilePath index_path =
      temp_dir.path().AppendASCII(SimpleIndexFile::kIndexFileName);
  EXPECT_TRUE(SimpleIndexFile::IsIndexFileStale(index_path));
  const std::string kDummyData = "nothing to be seen here";
  EXPECT_EQ(static_cast<int>(kDummyData.size()),
            file_util::WriteFile(index_path,
                                 kDummyData.data(),
                                 kDummyData.size()));
  EXPECT_FALSE(SimpleIndexFile::IsIndexFileStale(index_path));

  SimpleIndexFile simple_index_file(base::MessageLoopProxy::current(),
                                    base::MessageLoopProxy::current(),
                                    temp_dir.path());

  SimpleIndexFile::IndexCompletionCallback callback =
      base::Bind(&SimpleIndexFileTest::IndexCompletionCallback,
                 base::Unretained(this));

  simple_index_file.LoadIndexEntries(base::MessageLoopProxy::current(),
                                     callback);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(file_util::PathExists(index_path));
  ASSERT_TRUE(callback_result());
  EXPECT_TRUE(callback_result()->index_file_entries);
  EXPECT_TRUE(callback_result()->force_index_flush);
}

}  // namespace disk_cache
