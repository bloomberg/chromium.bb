// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/pickle.h"
#include "base/sha1.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "net/disk_cache/simple/simple_index.h"
#include "net/disk_cache/simple/simple_index_file.h"
#include "net/disk_cache/simple/simple_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int64 kTestLastUsedTimeInternal = 12345;
const base::Time kTestLastUsedTime =
    base::Time::FromInternalValue(kTestLastUsedTimeInternal);
const uint64 kTestEntrySize = 789;

}  // namespace

namespace disk_cache {

class EntryMetadataTest  : public testing::Test {
 public:
  EntryMetadata NewEntryMetadataWithValues() {
    return EntryMetadata(kTestLastUsedTime, kTestEntrySize);
  }

  void CheckEntryMetadataValues(const EntryMetadata& entry_metadata) {
    EXPECT_EQ(kTestLastUsedTime, entry_metadata.GetLastUsedTime());
    EXPECT_EQ(kTestEntrySize, entry_metadata.GetEntrySize());
  }
};

class TestSimpleIndexFile : public SimpleIndexFile,
                            public base::SupportsWeakPtr<TestSimpleIndexFile> {
 public:
  TestSimpleIndexFile()
      : SimpleIndexFile(NULL, NULL, base::FilePath()),
        get_index_entries_calls_(0),
        doom_entry_set_calls_(0),
        last_response_thread_(NULL),
        disk_writes_(0) {}

  virtual void LoadIndexEntries(
      scoped_refptr<base::SingleThreadTaskRunner> response_thread,
      const SimpleIndexFile::IndexCompletionCallback&
          completion_callback) OVERRIDE {
    last_response_thread_ = response_thread.get();
    completion_callback_ = completion_callback;
    ++get_index_entries_calls_;
  }

  virtual void WriteToDisk(const SimpleIndex::EntrySet& entry_set,
                           uint64 cache_size,
                           const base::TimeTicks& start,
                           bool app_on_background) OVERRIDE {
    disk_writes_++;
    disk_write_entry_set_ = entry_set;
  }

  virtual void DoomEntrySet(
      scoped_ptr<std::vector<uint64> > entry_hashes,
      const base::Callback<void(int)>& reply_callback) OVERRIDE {
    last_doom_entry_hashes_ = *entry_hashes.get();
    last_doom_reply_callback_ = reply_callback;
    ++doom_entry_set_calls_;
  }

  void GetAndResetDiskWriteEntrySet(SimpleIndex::EntrySet* entry_set) {
    entry_set->swap(disk_write_entry_set_);
  }

  const SimpleIndexFile::IndexCompletionCallback& completion_callback() const {
    return completion_callback_;
  }
  int get_index_entries_calls() const { return get_index_entries_calls_; }
  int disk_writes() const { return disk_writes_; }
  const std::vector<uint64>& last_doom_entry_hashes() const {
    return last_doom_entry_hashes_;
  }
  int doom_entry_set_calls() const { return doom_entry_set_calls_; }

 private:
  SimpleIndexFile::IndexCompletionCallback completion_callback_;
  int get_index_entries_calls_;
  std::vector<uint64> last_doom_entry_hashes_;
  int doom_entry_set_calls_;
  base::SingleThreadTaskRunner* last_response_thread_;
  base::Callback<void(int)> last_doom_reply_callback_;
  int disk_writes_;
  SimpleIndex::EntrySet disk_write_entry_set_;
};

class SimpleIndexTest  : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    scoped_ptr<TestSimpleIndexFile> index_file(new TestSimpleIndexFile());
    index_file_ = index_file->AsWeakPtr();
    index_.reset(new SimpleIndex(NULL, base::FilePath(),
                                 index_file.PassAs<SimpleIndexFile>()));

    index_->Initialize();
  }

  void WaitForTimeChange() {
    base::Time now(base::Time::Now());

    do {
      base::PlatformThread::YieldCurrentThread();
    } while (now == base::Time::Now());
  }

  // Redirect to allow single "friend" declaration in base class.
  bool GetEntryForTesting(const std::string& key, EntryMetadata* metadata) {
    const uint64 hash_key = simple_util::GetEntryHashKey(key);
    SimpleIndex::EntrySet::iterator it = index_->entries_set_.find(hash_key);
    if (index_->entries_set_.end() == it)
      return false;
    *metadata = it->second;
    return true;
  }

  void InsertIntoIndexFileReturn(const std::string& key,
                                 base::Time last_used_time,
                                 uint64 entry_size) {
    uint64 hash_key(simple_util::GetEntryHashKey(key));
    index_file_return_map_.insert(std::make_pair(
        hash_key, EntryMetadata(last_used_time, entry_size)));
  }

  void ReturnIndexFile() {
    scoped_ptr<SimpleIndex::EntrySet> map(new SimpleIndex::EntrySet);
    map->swap(index_file_return_map_);
    index_file_->completion_callback().Run(map.Pass(), false);
  }

  // Non-const for timer manipulation.
  SimpleIndex* index() { return index_.get(); }
  const TestSimpleIndexFile* index_file() const { return index_file_.get(); }

 protected:
  SimpleIndex::EntrySet index_file_return_map_;
  scoped_ptr<SimpleIndex> index_;
  base::WeakPtr<TestSimpleIndexFile> index_file_;
};

TEST_F(EntryMetadataTest, Basics) {
  EntryMetadata entry_metadata;
  EXPECT_EQ(base::Time::FromInternalValue(0), entry_metadata.GetLastUsedTime());
  EXPECT_EQ(size_t(0), entry_metadata.GetEntrySize());

  entry_metadata = NewEntryMetadataWithValues();
  CheckEntryMetadataValues(entry_metadata);

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
}

TEST_F(SimpleIndexTest, IndexSizeCorrectOnMerge) {
  typedef disk_cache::SimpleIndex::EntrySet EntrySet;
  index()->SetMaxSize(100);
  index()->Insert("two");
  index()->UpdateEntrySize("two", 2);
  index()->Insert("five");
  index()->UpdateEntrySize("five", 5);
  index()->Insert("seven");
  index()->UpdateEntrySize("seven", 7);
  EXPECT_EQ(14U, index()->cache_size_);
  {
    scoped_ptr<EntrySet> entries(new EntrySet());
    index()->MergeInitializingSet(entries.Pass(), false);
  }
  EXPECT_EQ(14U, index()->cache_size_);
  {
    scoped_ptr<EntrySet> entries(new EntrySet());
    const uint64 hash_key = simple_util::GetEntryHashKey("eleven");
    entries->insert(
        std::make_pair(hash_key, EntryMetadata(base::Time::Now(), 11)));
    index()->MergeInitializingSet(entries.Pass(), false);
  }
  EXPECT_EQ(25U, index()->cache_size_);
}

// State of index changes as expected with an insert and a remove.
TEST_F(SimpleIndexTest, BasicInsertRemove) {
  // Confirm blank state.
  EntryMetadata metadata;
  EXPECT_EQ(base::Time(), metadata.GetLastUsedTime());
  EXPECT_EQ(0ul, metadata.GetEntrySize());

  // Confirm state after insert.
  index()->Insert("key1");
  EXPECT_TRUE(GetEntryForTesting("key1", &metadata));
  base::Time now(base::Time::Now());
  EXPECT_LT(now - base::TimeDelta::FromMinutes(1), metadata.GetLastUsedTime());
  EXPECT_GT(now + base::TimeDelta::FromMinutes(1), metadata.GetLastUsedTime());
  EXPECT_EQ(0ul, metadata.GetEntrySize());

  // Confirm state after remove.
  metadata = EntryMetadata();
  index()->Remove("key1");
  EXPECT_FALSE(GetEntryForTesting("key1", &metadata));
  EXPECT_EQ(base::Time(), metadata.GetLastUsedTime());
  EXPECT_EQ(0ul, metadata.GetEntrySize());
}

TEST_F(SimpleIndexTest, Has) {
  // Confirm the base index has dispatched the request for index entries.
  EXPECT_TRUE(index_file_.get());
  EXPECT_EQ(1, index_file_->get_index_entries_calls());

  // Confirm "Has()" always returns true before the callback is called.
  EXPECT_TRUE(index()->Has("key1"));
  index()->Insert("key1");
  EXPECT_TRUE(index()->Has("key1"));
  index()->Remove("key1");
  // TODO(rdsmith): Maybe return false on explicitly removed entries?
  EXPECT_TRUE(index()->Has("key1"));

  ReturnIndexFile();

  // Confirm "Has() returns conditionally now.
  EXPECT_FALSE(index()->Has("key1"));
  index()->Insert("key1");
  EXPECT_TRUE(index()->Has("key1"));
  index()->Remove("key1");
}

TEST_F(SimpleIndexTest, UseIfExists) {
  // Confirm the base index has dispatched the request for index entries.
  EXPECT_TRUE(index_file_.get());
  EXPECT_EQ(1, index_file_->get_index_entries_calls());

  // Confirm "UseIfExists()" always returns true before the callback is called
  // and updates mod time if the entry was really there.
  EntryMetadata metadata1, metadata2;
  EXPECT_TRUE(index()->UseIfExists("key1"));
  EXPECT_FALSE(GetEntryForTesting("key1", &metadata1));
  index()->Insert("key1");
  EXPECT_TRUE(index()->UseIfExists("key1"));
  EXPECT_TRUE(GetEntryForTesting("key1", &metadata1));
  WaitForTimeChange();
  EXPECT_TRUE(GetEntryForTesting("key1", &metadata2));
  EXPECT_EQ(metadata1.GetLastUsedTime(), metadata2.GetLastUsedTime());
  EXPECT_TRUE(index()->UseIfExists("key1"));
  EXPECT_TRUE(GetEntryForTesting("key1", &metadata2));
  EXPECT_LT(metadata1.GetLastUsedTime(), metadata2.GetLastUsedTime());
  index()->Remove("key1");
  EXPECT_TRUE(index()->UseIfExists("key1"));

  ReturnIndexFile();

  // Confirm "UseIfExists() returns conditionally now
  EXPECT_FALSE(index()->UseIfExists("key1"));
  EXPECT_FALSE(GetEntryForTesting("key1", &metadata1));
  index()->Insert("key1");
  EXPECT_TRUE(index()->UseIfExists("key1"));
  EXPECT_TRUE(GetEntryForTesting("key1", &metadata1));
  WaitForTimeChange();
  EXPECT_TRUE(GetEntryForTesting("key1", &metadata2));
  EXPECT_EQ(metadata1.GetLastUsedTime(), metadata2.GetLastUsedTime());
  EXPECT_TRUE(index()->UseIfExists("key1"));
  EXPECT_TRUE(GetEntryForTesting("key1", &metadata2));
  EXPECT_LT(metadata1.GetLastUsedTime(), metadata2.GetLastUsedTime());
  index()->Remove("key1");
  EXPECT_FALSE(index()->UseIfExists("key1"));
}

TEST_F(SimpleIndexTest, UpdateEntrySize) {
  base::Time now(base::Time::Now());

  index()->SetMaxSize(1000);

  InsertIntoIndexFileReturn("key1",
                            now - base::TimeDelta::FromDays(2),
                            475u);
  ReturnIndexFile();

  EntryMetadata metadata;
  EXPECT_TRUE(GetEntryForTesting("key1", &metadata));
  EXPECT_EQ(now - base::TimeDelta::FromDays(2), metadata.GetLastUsedTime());
  EXPECT_EQ(475u, metadata.GetEntrySize());

  index()->UpdateEntrySize("key1", 600u);
  EXPECT_TRUE(GetEntryForTesting("key1", &metadata));
  EXPECT_EQ(600u, metadata.GetEntrySize());
  EXPECT_EQ(1, index()->GetEntryCount());
}

TEST_F(SimpleIndexTest, GetEntryCount) {
  EXPECT_EQ(0, index()->GetEntryCount());
  index()->Insert("key1");
  EXPECT_EQ(1, index()->GetEntryCount());
  index()->Insert("key2");
  EXPECT_EQ(2, index()->GetEntryCount());
  index()->Insert("key3");
  EXPECT_EQ(3, index()->GetEntryCount());
  index()->Insert("key3");
  EXPECT_EQ(3, index()->GetEntryCount());
  index()->Remove("key2");
  EXPECT_EQ(2, index()->GetEntryCount());
  index()->Insert("key4");
  EXPECT_EQ(3, index()->GetEntryCount());
  index()->Remove("key3");
  EXPECT_EQ(2, index()->GetEntryCount());
  index()->Remove("key3");
  EXPECT_EQ(2, index()->GetEntryCount());
  index()->Remove("key1");
  EXPECT_EQ(1, index()->GetEntryCount());
  index()->Remove("key4");
  EXPECT_EQ(0, index()->GetEntryCount());
}

// Confirm that we get the results we expect from a simple init.
TEST_F(SimpleIndexTest, BasicInit) {
  base::Time now(base::Time::Now());

  InsertIntoIndexFileReturn("key1",
                            now - base::TimeDelta::FromDays(2),
                            10u);
  InsertIntoIndexFileReturn("key2",
                            now - base::TimeDelta::FromDays(3),
                            100u);

  ReturnIndexFile();

  EntryMetadata metadata;
  EXPECT_TRUE(GetEntryForTesting("key1", &metadata));
  EXPECT_EQ(now - base::TimeDelta::FromDays(2), metadata.GetLastUsedTime());
  EXPECT_EQ(10ul, metadata.GetEntrySize());
  EXPECT_TRUE(GetEntryForTesting("key2", &metadata));
  EXPECT_EQ(now - base::TimeDelta::FromDays(3), metadata.GetLastUsedTime());
  EXPECT_EQ(100ul, metadata.GetEntrySize());
}

// Remove something that's going to come in from the loaded index.
TEST_F(SimpleIndexTest, RemoveBeforeInit) {
  index()->Remove("key1");

  InsertIntoIndexFileReturn("key1",
                            base::Time::Now() - base::TimeDelta::FromDays(2),
                            10u);
  ReturnIndexFile();

  EXPECT_FALSE(index()->Has("key1"));
}

// Insert something that's going to come in from the loaded index; correct
// result?
TEST_F(SimpleIndexTest, InsertBeforeInit) {
  index()->Insert("key1");

  InsertIntoIndexFileReturn("key1",
                            base::Time::Now() - base::TimeDelta::FromDays(2),
                            10u);
  ReturnIndexFile();

  EntryMetadata metadata;
  EXPECT_TRUE(GetEntryForTesting("key1", &metadata));
  base::Time now(base::Time::Now());
  EXPECT_LT(now - base::TimeDelta::FromMinutes(1), metadata.GetLastUsedTime());
  EXPECT_GT(now + base::TimeDelta::FromMinutes(1), metadata.GetLastUsedTime());
  EXPECT_EQ(0ul, metadata.GetEntrySize());
}

// Insert and Remove something that's going to come in from the loaded index.
TEST_F(SimpleIndexTest, InsertRemoveBeforeInit) {
  index()->Insert("key1");
  index()->Remove("key1");

  InsertIntoIndexFileReturn("key1",
                            base::Time::Now() - base::TimeDelta::FromDays(2),
                            10u);
  ReturnIndexFile();

  EXPECT_FALSE(index()->Has("key1"));
}

// Insert and Remove something that's going to come in from the loaded index.
TEST_F(SimpleIndexTest, RemoveInsertBeforeInit) {
  index()->Remove("key1");
  index()->Insert("key1");

  InsertIntoIndexFileReturn("key1",
                            base::Time::Now() - base::TimeDelta::FromDays(2),
                            10u);
  ReturnIndexFile();

  EntryMetadata metadata;
  EXPECT_TRUE(GetEntryForTesting("key1", &metadata));
  base::Time now(base::Time::Now());
  EXPECT_LT(now - base::TimeDelta::FromMinutes(1), metadata.GetLastUsedTime());
  EXPECT_GT(now + base::TimeDelta::FromMinutes(1), metadata.GetLastUsedTime());
  EXPECT_EQ(0ul, metadata.GetEntrySize());
}

// Do all above tests at once + a non-conflict to test for cross-key
// interactions.
TEST_F(SimpleIndexTest, AllInitConflicts) {
  base::Time now(base::Time::Now());

  index()->Remove("key1");
  InsertIntoIndexFileReturn("key1",
                            now - base::TimeDelta::FromDays(2),
                            10u);
  index()->Insert("key2");
  InsertIntoIndexFileReturn("key2",
                            now - base::TimeDelta::FromDays(3),
                            100u);
  index()->Insert("key3");
  index()->Remove("key3");
  InsertIntoIndexFileReturn("key3",
                            now - base::TimeDelta::FromDays(4),
                            1000u);
  index()->Remove("key4");
  index()->Insert("key4");
  InsertIntoIndexFileReturn("key4",
                            now - base::TimeDelta::FromDays(5),
                            10000u);
  InsertIntoIndexFileReturn("key5",
                            now - base::TimeDelta::FromDays(6),
                            100000u);

  ReturnIndexFile();

  EXPECT_FALSE(index()->Has("key1"));

  EntryMetadata metadata;
  EXPECT_TRUE(GetEntryForTesting("key2", &metadata));
  EXPECT_LT(now - base::TimeDelta::FromMinutes(1), metadata.GetLastUsedTime());
  EXPECT_GT(now + base::TimeDelta::FromMinutes(1), metadata.GetLastUsedTime());
  EXPECT_EQ(0ul, metadata.GetEntrySize());

  EXPECT_FALSE(index()->Has("key3"));

  EXPECT_TRUE(GetEntryForTesting("key4", &metadata));
  EXPECT_LT(now - base::TimeDelta::FromMinutes(1), metadata.GetLastUsedTime());
  EXPECT_GT(now + base::TimeDelta::FromMinutes(1), metadata.GetLastUsedTime());
  EXPECT_EQ(0ul, metadata.GetEntrySize());

  EXPECT_TRUE(GetEntryForTesting("key5", &metadata));
  EXPECT_EQ(now - base::TimeDelta::FromDays(6), metadata.GetLastUsedTime());
  EXPECT_EQ(100000u, metadata.GetEntrySize());
}

TEST_F(SimpleIndexTest, BasicEviction) {
  base::Time now(base::Time::Now());
  index()->SetMaxSize(1000);
  InsertIntoIndexFileReturn("key1",
                            now - base::TimeDelta::FromDays(2),
                            475u);
  index()->Insert("key2");
  index()->UpdateEntrySize("key2", 475);
  ReturnIndexFile();

  WaitForTimeChange();

  index()->Insert("key3");
  // Confirm index is as expected: No eviction, everything there.
  EXPECT_EQ(3, index()->GetEntryCount());
  EXPECT_EQ(0, index_file()->doom_entry_set_calls());
  EXPECT_TRUE(index()->Has("key1"));
  EXPECT_TRUE(index()->Has("key2"));
  EXPECT_TRUE(index()->Has("key3"));

  // Trigger an eviction, and make sure the right things are tossed.
  // TODO(rdsmith): This is dependent on the innards of the implementation
  // as to at exactly what point we trigger eviction.  Not sure how to fix
  // that.
  index()->UpdateEntrySize("key3", 475);
  EXPECT_EQ(1, index_file()->doom_entry_set_calls());
  EXPECT_EQ(1, index()->GetEntryCount());
  EXPECT_FALSE(index()->Has("key1"));
  EXPECT_FALSE(index()->Has("key2"));
  EXPECT_TRUE(index()->Has("key3"));
  ASSERT_EQ(2u, index_file_->last_doom_entry_hashes().size());
}

// Confirm all the operations queue a disk write at some point in the
// future.
TEST_F(SimpleIndexTest, DiskWriteQueued) {
  index()->SetMaxSize(1000);
  ReturnIndexFile();

  EXPECT_FALSE(index()->write_to_disk_timer_.IsRunning());

  index()->Insert("key1");
  EXPECT_TRUE(index()->write_to_disk_timer_.IsRunning());
  index()->write_to_disk_timer_.Stop();
  EXPECT_FALSE(index()->write_to_disk_timer_.IsRunning());

  index()->UseIfExists("key1");
  EXPECT_TRUE(index()->write_to_disk_timer_.IsRunning());
  index()->write_to_disk_timer_.Stop();

  index()->UpdateEntrySize("key1", 20);
  EXPECT_TRUE(index()->write_to_disk_timer_.IsRunning());
  index()->write_to_disk_timer_.Stop();

  index()->Remove("key1");
  EXPECT_TRUE(index()->write_to_disk_timer_.IsRunning());
  index()->write_to_disk_timer_.Stop();
}

TEST_F(SimpleIndexTest, DiskWriteExecuted) {
  index()->SetMaxSize(1000);
  ReturnIndexFile();

  EXPECT_FALSE(index()->write_to_disk_timer_.IsRunning());

  index()->Insert("key1");
  index()->UpdateEntrySize("key1", 20);
  EXPECT_TRUE(index()->write_to_disk_timer_.IsRunning());
  base::Closure user_task(index()->write_to_disk_timer_.user_task());
  index()->write_to_disk_timer_.Stop();

  EXPECT_EQ(0, index_file_->disk_writes());
  user_task.Run();
  EXPECT_EQ(1, index_file_->disk_writes());
  SimpleIndex::EntrySet entry_set;
  index_file_->GetAndResetDiskWriteEntrySet(&entry_set);

  uint64 hash_key(simple_util::GetEntryHashKey("key1"));
  base::Time now(base::Time::Now());
  ASSERT_EQ(1u, entry_set.size());
  EXPECT_EQ(hash_key, entry_set.begin()->first);
  const EntryMetadata& entry1(entry_set.begin()->second);
  EXPECT_LT(now - base::TimeDelta::FromMinutes(1), entry1.GetLastUsedTime());
  EXPECT_GT(now + base::TimeDelta::FromMinutes(1), entry1.GetLastUsedTime());
  EXPECT_EQ(20u, entry1.GetEntrySize());
}

TEST_F(SimpleIndexTest, DiskWritePostponed) {
  index()->SetMaxSize(1000);
  ReturnIndexFile();

  EXPECT_FALSE(index()->write_to_disk_timer_.IsRunning());

  index()->Insert("key1");
  index()->UpdateEntrySize("key1", 20);
  EXPECT_TRUE(index()->write_to_disk_timer_.IsRunning());
  base::TimeTicks expected_trigger(
      index()->write_to_disk_timer_.desired_run_time());

  WaitForTimeChange();
  EXPECT_EQ(expected_trigger, index()->write_to_disk_timer_.desired_run_time());
  index()->Insert("key2");
  index()->UpdateEntrySize("key2", 40);
  EXPECT_TRUE(index()->write_to_disk_timer_.IsRunning());
  EXPECT_LT(expected_trigger, index()->write_to_disk_timer_.desired_run_time());
  index()->write_to_disk_timer_.Stop();
}

}  // namespace disk_cache
