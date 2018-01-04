// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/dom_storage/dom_storage_area.h"
#include "content/browser/dom_storage/dom_storage_database.h"
#include "content/browser/dom_storage/dom_storage_database_adapter.h"
#include "content/browser/dom_storage/dom_storage_task_runner.h"
#include "content/browser/dom_storage/local_storage_database_adapter.h"
#include "content/browser/dom_storage/session_storage_database.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace content {

class DOMStorageAreaTest : public testing::Test {
 public:
  DOMStorageAreaTest()
    : kOrigin(GURL("http://dom_storage/")),
      kKey(ASCIIToUTF16("key")),
      kValue(ASCIIToUTF16("value")),
      kKey2(ASCIIToUTF16("key2")),
      kValue2(ASCIIToUTF16("value2")) {
  }

  const GURL kOrigin;
  const base::string16 kKey;
  const base::string16 kValue;
  const base::string16 kKey2;
  const base::string16 kValue2;

  // Method used in the CommitTasks test case.
  void InjectedCommitSequencingTask1(
      const scoped_refptr<DOMStorageArea>& area) {
    // At this point the StartCommitTimer task has run and
    // the OnCommitTimer task is queued. We want to inject after
    // that.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&DOMStorageAreaTest::InjectedCommitSequencingTask2,
                       base::Unretained(this), area));
  }

  void InjectedCommitSequencingTask2(
      const scoped_refptr<DOMStorageArea>& area) {
    // At this point the OnCommitTimer has run.
    // Verify that it put a commit in flight.
    EXPECT_TRUE(area->HasCommitBatchInFlight());
    EXPECT_FALSE(area->GetCurrentCommitBatch());
    EXPECT_TRUE(area->HasUncommittedChanges());
    // Make additional change and verify that a new commit batch
    // is created for that change.
    base::NullableString16 old_value;
    EXPECT_TRUE(area->SetItem(kKey2, kValue2, old_value, &old_value));
    EXPECT_TRUE(area->GetCurrentCommitBatch());
    EXPECT_TRUE(area->HasCommitBatchInFlight());
    EXPECT_TRUE(area->HasUncommittedChanges());
  }

  // Class used in the CommitChangesAtShutdown test case.
  class VerifyChangesCommittedDatabase : public DOMStorageDatabase {
   public:
    VerifyChangesCommittedDatabase() {}
    ~VerifyChangesCommittedDatabase() override {
      const base::string16 kKey(ASCIIToUTF16("key"));
      const base::string16 kValue(ASCIIToUTF16("value"));
      DOMStorageValuesMap values;
      ReadAllValues(&values);
      EXPECT_EQ(1u, values.size());
      EXPECT_EQ(kValue, values[kKey].string());
    }
  };

 private:
  base::MessageLoop message_loop_;
};

class DOMStorageAreaParamTest : public DOMStorageAreaTest,
                                public testing::WithParamInterface<bool> {
 public:
  DOMStorageAreaParamTest() {}
  ~DOMStorageAreaParamTest() override {}
};

INSTANTIATE_TEST_CASE_P(_, DOMStorageAreaParamTest, ::testing::Bool());

TEST_P(DOMStorageAreaParamTest, DOMStorageAreaBasics) {
  scoped_refptr<DOMStorageArea> area(
      new DOMStorageArea(1, std::string(), nullptr, kOrigin, nullptr, nullptr));
  const bool values_cached = GetParam();
  area->SetCacheOnlyKeys(!values_cached);
  base::string16 old_value;
  base::NullableString16 old_nullable_value;
  scoped_refptr<DOMStorageArea> copy;

  // We don't focus on the underlying DOMStorageMap functionality
  // since that's covered by seperate unit tests.
  EXPECT_EQ(kOrigin, area->origin());
  EXPECT_EQ(1, area->namespace_id());
  EXPECT_EQ(0u, area->Length());
  EXPECT_TRUE(
      area->SetItem(kKey, kValue, old_nullable_value, &old_nullable_value));
  EXPECT_TRUE(
      area->SetItem(kKey2, kValue2, old_nullable_value, &old_nullable_value));
  EXPECT_FALSE(area->HasUncommittedChanges());

  // Verify that a copy shares the same map.
  copy = area->ShallowCopy(2, std::string());
  EXPECT_EQ(kOrigin, copy->origin());
  EXPECT_EQ(2, copy->namespace_id());
  EXPECT_EQ(area->Length(), copy->Length());
  if (values_cached)
    EXPECT_EQ(area->GetItem(kKey).string(), copy->GetItem(kKey).string());
  EXPECT_EQ(area->Key(0).string(), copy->Key(0).string());
  EXPECT_EQ(copy->map_.get(), area->map_.get());
  copy->ClearShallowCopiedCommitBatches();

  // But will deep copy-on-write as needed.
  old_nullable_value = base::NullableString16(kValue, false);
  EXPECT_TRUE(area->RemoveItem(kKey, old_nullable_value, &old_value));
  EXPECT_EQ(kValue, old_value);
  EXPECT_NE(copy->map_.get(), area->map_.get());
  copy = area->ShallowCopy(2, std::string());
  EXPECT_EQ(copy->map_.get(), area->map_.get());
  EXPECT_TRUE(
      area->SetItem(kKey, kValue, old_nullable_value, &old_nullable_value));
  EXPECT_NE(copy->map_.get(), area->map_.get());
  copy = area->ShallowCopy(2, std::string());
  EXPECT_EQ(copy->map_.get(), area->map_.get());
  EXPECT_NE(0u, area->Length());
  EXPECT_TRUE(area->Clear());
  EXPECT_EQ(0u, area->Length());
  EXPECT_NE(copy->map_.get(), area->map_.get());

  // Verify that once Shutdown(), behaves that way.
  area->Shutdown();
  EXPECT_TRUE(area->is_shutdown_);
  EXPECT_FALSE(area->map_.get());
  EXPECT_EQ(0u, area->Length());
  EXPECT_TRUE(area->Key(0).is_null());
  if (values_cached)
    EXPECT_TRUE(area->GetItem(kKey).is_null());
  EXPECT_FALSE(
      area->SetItem(kKey, kValue, old_nullable_value, &old_nullable_value));
  EXPECT_FALSE(area->RemoveItem(kKey, old_nullable_value, &old_value));
  EXPECT_FALSE(area->Clear());
}

TEST_F(DOMStorageAreaTest, BackingDatabaseOpened) {
  const int64_t kSessionStorageNamespaceId = kLocalStorageNamespaceId + 1;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath kExpectedOriginFilePath = temp_dir.GetPath().Append(
      DOMStorageArea::DatabaseFileNameFromOrigin(kOrigin));

  // No directory, backing should be null.
  {
    scoped_refptr<DOMStorageArea> area(
        new DOMStorageArea(kOrigin, base::FilePath(), nullptr));
    EXPECT_EQ(nullptr, area->backing_.get());
    EXPECT_EQ(DOMStorageArea::LOAD_STATE_KEYS_AND_VALUES, area->load_state_);
    EXPECT_FALSE(base::PathExists(kExpectedOriginFilePath));
  }

  // Valid directory and origin but no session storage backing. Backing should
  // be null.
  {
    scoped_refptr<DOMStorageArea> area(
        new DOMStorageArea(kSessionStorageNamespaceId, std::string(), nullptr,
                           kOrigin, nullptr, nullptr));
    EXPECT_EQ(nullptr, area->backing_.get());

    base::NullableString16 old_value;
    EXPECT_TRUE(area->SetItem(kKey, kValue, old_value, &old_value));
    ASSERT_TRUE(old_value.is_null());

    // Check that saving a value has still left us without a backing database.
    EXPECT_EQ(nullptr, area->backing_.get());
    EXPECT_FALSE(base::PathExists(kExpectedOriginFilePath));
  }

  // This should set up a DOMStorageArea that is correctly backed to disk.
  {
    scoped_refptr<DOMStorageArea> area(
        new DOMStorageArea(kOrigin, temp_dir.GetPath(),
                           new MockDOMStorageTaskRunner(
                               base::ThreadTaskRunnerHandle::Get().get())));

    EXPECT_TRUE(area->backing_.get());
    DOMStorageDatabase* database = static_cast<LocalStorageDatabaseAdapter*>(
        area->backing_.get())->db_.get();
    EXPECT_FALSE(database->IsOpen());
    EXPECT_EQ(DOMStorageArea::LOAD_STATE_UNLOADED, area->load_state_);

    // Inject an in-memory db to speed up the test.
    // We will verify that something is written into the database but not
    // that a file is written to disk - DOMStorageDatabase unit tests cover
    // that.
    area->backing_.reset(new LocalStorageDatabaseAdapter());

    // Need to write something to ensure that the database is created.
    base::NullableString16 old_value;
    EXPECT_TRUE(area->SetItem(kKey, kValue, old_value, &old_value));
    ASSERT_TRUE(old_value.is_null());
    EXPECT_EQ(area->desired_load_state_, area->load_state_);
    EXPECT_TRUE(area->GetCurrentCommitBatch());
    EXPECT_FALSE(area->HasCommitBatchInFlight());

    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(area->GetCurrentCommitBatch());
    EXPECT_FALSE(area->HasCommitBatchInFlight());
    database = static_cast<LocalStorageDatabaseAdapter*>(
        area->backing_.get())->db_.get();
    EXPECT_TRUE(database->IsOpen());
    EXPECT_EQ(1u, area->Length());
    EXPECT_EQ(kKey, area->Key(0).string());
  }
}

TEST_P(DOMStorageAreaParamTest, ShallowCopyWithBacking) {
  base::ScopedTempDir temp_dir;
  const std::string kNamespaceId = "id1";
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  scoped_refptr<SessionStorageDatabase> db = new SessionStorageDatabase(
      temp_dir.GetPath(), base::ThreadTaskRunnerHandle::Get());
  scoped_refptr<DOMStorageArea> area(new DOMStorageArea(
      1, kNamespaceId, nullptr, kOrigin, db.get(),
      new MockDOMStorageTaskRunner(base::ThreadTaskRunnerHandle::Get().get())));
  EXPECT_TRUE(area->backing_.get());
  EXPECT_TRUE(area->session_storage_backing_);
  const bool values_cached = GetParam();
  area->SetCacheOnlyKeys(!values_cached);

  scoped_refptr<DOMStorageArea> temp_copy;
  temp_copy = area->ShallowCopy(2, std::string());
  EXPECT_TRUE(temp_copy->commit_batches_.empty());
  temp_copy->ClearShallowCopiedCommitBatches();

  // Check if shallow copy is consistent.
  base::string16 old_value;
  base::NullableString16 old_nullable_value;
  scoped_refptr<DOMStorageArea> copy;
  EXPECT_TRUE(
      area->SetItem(kKey, kValue, old_nullable_value, &old_nullable_value));
  EXPECT_TRUE(area->HasUncommittedChanges());
  EXPECT_EQ(DOMStorageArea::CommitBatchHolder::TYPE_CURRENT_BATCH,
            area->commit_batches_.front().type);
  copy = area->ShallowCopy(3, std::string());
  EXPECT_EQ(copy->map_.get(), area->map_.get());
  EXPECT_EQ(1u, copy->original_persistent_namespace_ids_->size());
  EXPECT_EQ(kNamespaceId, (*copy->original_persistent_namespace_ids_)[0]);
  if (!values_cached) {
    EXPECT_EQ(area->commit_batches_.front().batch,
              copy->commit_batches_.front().batch);
    EXPECT_EQ(DOMStorageArea::CommitBatchHolder::TYPE_IN_FLIGHT,
              area->commit_batches_.front().type);
    EXPECT_EQ(DOMStorageArea::CommitBatchHolder::TYPE_CLONE,
              copy->commit_batches_.front().type);
  } else {
    EXPECT_TRUE(copy->commit_batches_.empty());
  }

  DOMStorageValuesMap map;
  copy->ExtractValues(&map);
  EXPECT_EQ(1u, map.size());
  EXPECT_EQ(kValue, map[kKey].string());

  // Check if copy on write works.
  EXPECT_TRUE(
      copy->SetItem(kKey2, kValue2, old_nullable_value, &old_nullable_value));
  EXPECT_TRUE(copy->GetCurrentCommitBatch());
  EXPECT_FALSE(copy->commit_batches_.front().type);
  if (!values_cached)
    EXPECT_EQ(DOMStorageArea::CommitBatchHolder::TYPE_CLONE,
              copy->commit_batches_.back().type);
  else
    EXPECT_FALSE(copy->HasCommitBatchInFlight());
  EXPECT_EQ(1u, area->Length());

  // Check clearing of cloned batches.
  area->ClearShallowCopiedCommitBatches();
  copy->ClearShallowCopiedCommitBatches();
  EXPECT_EQ(DOMStorageArea::CommitBatchHolder::TYPE_IN_FLIGHT,
            area->commit_batches_.front().type);
  EXPECT_FALSE(copy->HasCommitBatchInFlight());
}

TEST_F(DOMStorageAreaTest, SetCacheOnlyKeysWithoutBacking) {
  scoped_refptr<DOMStorageArea> area(
      new DOMStorageArea(1, std::string(), nullptr, kOrigin, nullptr, nullptr));
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_KEYS_AND_VALUES,
            area->desired_load_state_);
  EXPECT_FALSE(area->map_->has_only_keys());
  base::NullableString16 old_value;
  EXPECT_TRUE(area->SetItem(kKey, kValue, old_value, &old_value));
  DOMStorageValuesMap map;
  area->ExtractValues(&map);
  EXPECT_EQ(1u, map.size());

  area->SetCacheOnlyKeys(true);
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_KEYS_AND_VALUES,
            area->desired_load_state_);  // cannot be disabled without backing.
  area->SetItem(kKey, kValue2, old_value, &old_value);
  EXPECT_FALSE(area->map_->has_only_keys());
  EXPECT_EQ(kValue, old_value.string());
  area->ExtractValues(&map);
  EXPECT_EQ(kValue2, map[kKey].string());
  EXPECT_EQ(1u, map.size());
}

TEST_F(DOMStorageAreaTest, SetCacheOnlyKeysWithBacking) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath kExpectedOriginFilePath = temp_dir.GetPath().Append(
      DOMStorageArea::DatabaseFileNameFromOrigin(kOrigin));
  scoped_refptr<DOMStorageArea> area(new DOMStorageArea(
      kOrigin, temp_dir.GetPath(),
      new MockDOMStorageTaskRunner(base::ThreadTaskRunnerHandle::Get().get())));

  EXPECT_TRUE(area->backing_.get());
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_UNLOADED, area->load_state_);
  area->backing_.reset(new LocalStorageDatabaseAdapter());

#if !defined(OS_ANDROID)
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_KEYS_AND_VALUES,
            area->desired_load_state_);
  area->SetCacheOnlyKeys(true);
#endif
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_KEYS_ONLY, area->desired_load_state_);
  base::NullableString16 old_value;
  EXPECT_TRUE(area->SetItem(kKey, kValue, old_value, &old_value));
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_KEYS_ONLY, area->load_state_);
  EXPECT_TRUE(area->GetCurrentCommitBatch());

  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(area->GetCurrentCommitBatch());
  EXPECT_EQ(1u, area->Length());

  // Fill the current batch and in flight batch.
  EXPECT_TRUE(area->SetItem(kKey2, kValue, old_value, &old_value));
  area->PostCommitTask();
  EXPECT_FALSE(area->GetCurrentCommitBatch());
  EXPECT_TRUE(area->HasCommitBatchInFlight());
  EXPECT_TRUE(area->SetItem(kKey2, kValue2, old_value, &old_value));
  EXPECT_TRUE(area->GetCurrentCommitBatch());

  // The values must be imported from the backing, and from the commit batches.
  area->SetCacheOnlyKeys(false);
  EXPECT_EQ(2u, area->Length());
  EXPECT_EQ(kValue, area->GetItem(kKey).string());
  EXPECT_EQ(kValue2, area->GetItem(kKey2).string());

  // Check if disabling cache clears the cache after committing only.
  area->SetCacheOnlyKeys(true);
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_KEYS_ONLY, area->desired_load_state_);
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_KEYS_AND_VALUES, area->load_state_);
  ASSERT_FALSE(area->map_->has_only_keys());

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_KEYS_ONLY, area->load_state_);
  EXPECT_TRUE(area->map_->has_only_keys());
  EXPECT_FALSE(area->HasCommitBatchInFlight());

  // Check if Clear() works as expected when values are desired.
  area->Clear();
  EXPECT_TRUE(area->SetItem(kKey2, kValue2, old_value, &old_value));
  area->PostCommitTask();
  EXPECT_TRUE(area->SetItem(kKey, kValue, old_value, &old_value));
  EXPECT_EQ(2u, area->Length());
  area->Clear();
  EXPECT_TRUE(area->SetItem(kKey2, kValue, old_value, &old_value));
  EXPECT_TRUE(area->GetCurrentCommitBatch()->batch->clear_all_first);
  EXPECT_TRUE(area->commit_batches_.back().batch->clear_all_first);
  area->SetCacheOnlyKeys(false);
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_KEYS_ONLY, area->load_state_);

  // Unload only after commit.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_UNLOADED, area->load_state_);
  EXPECT_EQ(1u, area->Length());
  EXPECT_EQ(kValue, area->GetItem(kKey2).string());
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_KEYS_AND_VALUES, area->load_state_);
}

TEST_P(DOMStorageAreaParamTest, CommitTasks) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  scoped_refptr<DOMStorageArea> area(new DOMStorageArea(
      kOrigin, temp_dir.GetPath(),
      new MockDOMStorageTaskRunner(base::ThreadTaskRunnerHandle::Get().get())));
  // Inject an in-memory db to speed up the test.
  area->backing_.reset(new LocalStorageDatabaseAdapter());
  area->SetCacheOnlyKeys(GetParam());

  // Unrelated to commits, but while we're here, see that querying Length()
  // causes the backing database to be opened and presumably read from.
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_UNLOADED, area->load_state_);
  EXPECT_EQ(0u, area->Length());
  EXPECT_EQ(area->desired_load_state_, area->load_state_);

  DOMStorageValuesMap values;
  base::NullableString16 old_value;

  // See that changes are batched up.
  EXPECT_FALSE(area->GetCurrentCommitBatch());
  EXPECT_TRUE(area->SetItem(kKey, kValue, old_value, &old_value));
  EXPECT_TRUE(area->HasUncommittedChanges());
  EXPECT_TRUE(area->GetCurrentCommitBatch());
  EXPECT_FALSE(area->GetCurrentCommitBatch()->batch->clear_all_first);
  EXPECT_EQ(1u, area->GetCurrentCommitBatch()->batch->changed_values.size());
  EXPECT_TRUE(area->SetItem(kKey2, kValue2, old_value, &old_value));
  EXPECT_FALSE(area->GetCurrentCommitBatch()->batch->clear_all_first);
  EXPECT_EQ(2u, area->GetCurrentCommitBatch()->batch->changed_values.size());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(area->HasUncommittedChanges());
  EXPECT_FALSE(area->GetCurrentCommitBatch());
  EXPECT_FALSE(area->HasCommitBatchInFlight());
  // Verify the changes made it to the database.
  values.clear();
  area->backing_->ReadAllValues(&values);
  EXPECT_EQ(2u, values.size());
  EXPECT_EQ(kValue, values[kKey].string());
  EXPECT_EQ(kValue2, values[kKey2].string());

  // See that clear is handled properly.
  EXPECT_TRUE(area->Clear());
  EXPECT_TRUE(area->GetCurrentCommitBatch());
  EXPECT_TRUE(area->GetCurrentCommitBatch()->batch->clear_all_first);
  EXPECT_TRUE(area->GetCurrentCommitBatch()->batch->changed_values.empty());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(area->GetCurrentCommitBatch());
  EXPECT_FALSE(area->HasCommitBatchInFlight());
  // Verify the changes made it to the database.
  values.clear();
  area->backing_->ReadAllValues(&values);
  EXPECT_TRUE(values.empty());

  // See that if changes accrue while a commit is "in flight"
  // those will also get committed.
  EXPECT_TRUE(area->SetItem(kKey, kValue, old_value, &old_value));
  EXPECT_TRUE(area->HasUncommittedChanges());
  // At this point the StartCommitTimer task has been posted to the after
  // startup task queue. We inject another task in the queue that will
  // execute when the CommitChanges task is inflight. From within our
  // injected task, we'll make an additional SetItem() call and verify
  // that a new commit batch is created for that additional change.
  BrowserThread::PostAfterStartupTask(
      FROM_HERE, base::ThreadTaskRunnerHandle::Get(),
      base::BindOnce(&DOMStorageAreaTest::InjectedCommitSequencingTask1,
                     base::Unretained(this), area));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(area->HasOneRef());
  EXPECT_FALSE(area->HasUncommittedChanges());
  // Verify the changes made it to the database.
  values.clear();
  area->backing_->ReadAllValues(&values);
  EXPECT_EQ(2u, values.size());
  EXPECT_EQ(kValue, values[kKey].string());
  EXPECT_EQ(kValue2, values[kKey2].string());
}

TEST_P(DOMStorageAreaParamTest, CommitChangesAtShutdown) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  scoped_refptr<DOMStorageArea> area(new DOMStorageArea(
      kOrigin, temp_dir.GetPath(),
      new MockDOMStorageTaskRunner(base::ThreadTaskRunnerHandle::Get().get())));

  // Inject an in-memory db to speed up the test and also to verify
  // the final changes are commited in it's dtor.
  static_cast<LocalStorageDatabaseAdapter*>(area->backing_.get())->db_.reset(
      new VerifyChangesCommittedDatabase());
  area->SetCacheOnlyKeys(GetParam());

  DOMStorageValuesMap values;
  base::NullableString16 old_value;
  EXPECT_TRUE(area->SetItem(kKey, kValue, old_value, &old_value));
  EXPECT_TRUE(area->HasUncommittedChanges());
  area->backing_->ReadAllValues(&values);
  EXPECT_TRUE(values.empty());  // not committed yet
  area->Shutdown();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(area->HasOneRef());
  EXPECT_FALSE(area->backing_.get());
  // The VerifyChangesCommittedDatabase destructor verifies values
  // were committed.

  // A second Shutdown call should be safe.
  area->Shutdown();
}

TEST_P(DOMStorageAreaParamTest, DeleteOrigin) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  scoped_refptr<DOMStorageArea> area(new DOMStorageArea(
      kOrigin, temp_dir.GetPath(),
      new MockDOMStorageTaskRunner(base::ThreadTaskRunnerHandle::Get().get())));
  area->SetCacheOnlyKeys(GetParam());

  // This test puts files on disk.
  base::FilePath db_file_path = static_cast<LocalStorageDatabaseAdapter*>(
      area->backing_.get())->db_->file_path();
  base::FilePath db_journal_file_path =
      DOMStorageDatabase::GetJournalFilePath(db_file_path);

  // Nothing bad should happen when invoked w/o any files on disk.
  area->DeleteOrigin();
  EXPECT_FALSE(base::PathExists(db_file_path));

  // Commit something in the database and then delete.
  base::NullableString16 old_value;
  area->SetItem(kKey, kValue, old_value, &old_value);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(base::PathExists(db_file_path));
  area->DeleteOrigin();
  EXPECT_EQ(0u, area->Length());
  EXPECT_FALSE(base::PathExists(db_file_path));
  EXPECT_FALSE(base::PathExists(db_journal_file_path));

  // Put some uncommitted changes to a non-existing database in
  // and then delete. No file ever gets created in this case.
  area->SetItem(kKey, kValue, old_value, &old_value);
  EXPECT_TRUE(area->HasUncommittedChanges());
  EXPECT_EQ(1u, area->Length());
  area->DeleteOrigin();
  EXPECT_TRUE(area->HasUncommittedChanges());
  EXPECT_EQ(0u, area->Length());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(area->HasUncommittedChanges());
  EXPECT_FALSE(base::PathExists(db_file_path));

  // Put some uncommitted changes to a an existing database in
  // and then delete.
  area->SetItem(kKey, kValue, old_value, &old_value);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(base::PathExists(db_file_path));
  area->SetItem(kKey2, kValue2, old_value, &old_value);
  EXPECT_TRUE(area->HasUncommittedChanges());
  EXPECT_EQ(2u, area->Length());
  area->DeleteOrigin();
  EXPECT_TRUE(area->HasUncommittedChanges());
  EXPECT_EQ(0u, area->Length());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(area->HasUncommittedChanges());
  // Since the area had uncommitted changes at the time delete
  // was called, the file will linger until the shutdown time.
  EXPECT_TRUE(base::PathExists(db_file_path));
  area->Shutdown();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(base::PathExists(db_file_path));

  // A second Shutdown call should be safe.
  area->Shutdown();
}

TEST_P(DOMStorageAreaParamTest, PurgeMemory) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  scoped_refptr<DOMStorageArea> area(new DOMStorageArea(
      kOrigin, temp_dir.GetPath(),
      new MockDOMStorageTaskRunner(base::ThreadTaskRunnerHandle::Get().get())));
  area->SetCacheOnlyKeys(GetParam());

  // Inject an in-memory db to speed up the test.
  area->backing_.reset(new LocalStorageDatabaseAdapter());

  // Unowned ptrs we use to verify that 'purge' has happened.
  DOMStorageDatabase* original_backing =
      static_cast<LocalStorageDatabaseAdapter*>(
          area->backing_.get())->db_.get();
  DOMStorageMap* original_map = area->map_.get();

  // Should do no harm when called on a newly constructed object.
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_UNLOADED, area->load_state_);
  area->PurgeMemory();
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_UNLOADED, area->load_state_);
  DOMStorageDatabase* new_backing = static_cast<LocalStorageDatabaseAdapter*>(
      area->backing_.get())->db_.get();
  EXPECT_EQ(original_backing, new_backing);
  EXPECT_EQ(original_map, area->map_.get());

  // Should not do anything when commits are pending.
  base::NullableString16 old_value;
  area->SetItem(kKey, kValue, old_value, &old_value);
  original_map = area->map_.get();  // importing creates new map.
  EXPECT_EQ(area->desired_load_state_, area->load_state_);
  EXPECT_TRUE(area->HasUncommittedChanges());
  area->PurgeMemory();
  EXPECT_EQ(area->desired_load_state_, area->load_state_);
  EXPECT_TRUE(area->HasUncommittedChanges());
  new_backing = static_cast<LocalStorageDatabaseAdapter*>(
      area->backing_.get())->db_.get();
  EXPECT_EQ(original_backing, new_backing);
  EXPECT_EQ(original_map, area->map_.get());

  // Commit the changes from above,
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(area->HasUncommittedChanges());
  new_backing = static_cast<LocalStorageDatabaseAdapter*>(
      area->backing_.get())->db_.get();
  EXPECT_EQ(original_backing, new_backing);
  EXPECT_EQ(original_map, area->map_.get());

  // Should drop caches and reset database connections
  // when invoked on an area that's loaded up primed.
  area->PurgeMemory();
  EXPECT_EQ(DOMStorageArea::LOAD_STATE_UNLOADED, area->load_state_);
  new_backing = static_cast<LocalStorageDatabaseAdapter*>(
      area->backing_.get())->db_.get();
  EXPECT_NE(original_backing, new_backing);
  EXPECT_NE(original_map, area->map_.get());
}

TEST_F(DOMStorageAreaTest, DatabaseFileNames) {
  struct {
    const char* origin;
    const char* file_name;
    const char* journal_file_name;
  } kCases[] = {
    { "https://www.google.com/",
      "https_www.google.com_0.localstorage",
      "https_www.google.com_0.localstorage-journal" },
    { "http://www.google.com:8080/",
      "http_www.google.com_8080.localstorage",
      "http_www.google.com_8080.localstorage-journal" },
    { "file:///",
      "file__0.localstorage",
      "file__0.localstorage-journal" },
  };

  for (size_t i = 0; i < arraysize(kCases); ++i) {
    GURL origin = GURL(kCases[i].origin).GetOrigin();
    base::FilePath file_name =
        base::FilePath().AppendASCII(kCases[i].file_name);
    base::FilePath journal_file_name =
        base::FilePath().AppendASCII(kCases[i].journal_file_name);

    EXPECT_EQ(file_name,
              DOMStorageArea::DatabaseFileNameFromOrigin(origin));
    EXPECT_EQ(origin,
              DOMStorageArea::OriginFromDatabaseFileName(file_name));
    EXPECT_EQ(journal_file_name,
              DOMStorageDatabase::GetJournalFilePath(file_name));
  }

  // Also test some DOMStorageDatabase::GetJournalFilePath cases here.
  base::FilePath parent = base::FilePath().AppendASCII("a").AppendASCII("b");
  EXPECT_EQ(
      parent.AppendASCII("file-journal"),
      DOMStorageDatabase::GetJournalFilePath(parent.AppendASCII("file")));
  EXPECT_EQ(
      base::FilePath().AppendASCII("-journal"),
      DOMStorageDatabase::GetJournalFilePath(base::FilePath()));
  EXPECT_EQ(
      base::FilePath().AppendASCII(".extensiononly-journal"),
      DOMStorageDatabase::GetJournalFilePath(
          base::FilePath().AppendASCII(".extensiononly")));
}

TEST_F(DOMStorageAreaTest, RateLimiter) {
  // Limit to 1000 samples per second
  DOMStorageArea::RateLimiter rate_limiter(
      1000, base::TimeDelta::FromSeconds(1));

  // No samples have been added so no time/delay should be needed.
  EXPECT_EQ(base::TimeDelta(),
            rate_limiter.ComputeTimeNeeded());
  EXPECT_EQ(base::TimeDelta(),
            rate_limiter.ComputeDelayNeeded(base::TimeDelta()));
  EXPECT_EQ(base::TimeDelta(),
            rate_limiter.ComputeDelayNeeded(base::TimeDelta::FromDays(1)));

  // Add a seconds worth of samples.
  rate_limiter.add_samples(1000);
  EXPECT_EQ(base::TimeDelta::FromSeconds(1),
            rate_limiter.ComputeTimeNeeded());
  EXPECT_EQ(base::TimeDelta::FromSeconds(1),
            rate_limiter.ComputeDelayNeeded(base::TimeDelta()));
  EXPECT_EQ(base::TimeDelta(),
            rate_limiter.ComputeDelayNeeded(base::TimeDelta::FromSeconds(1)));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(250),
            rate_limiter.ComputeDelayNeeded(
                base::TimeDelta::FromMilliseconds(750)));
  EXPECT_EQ(base::TimeDelta(),
            rate_limiter.ComputeDelayNeeded(
                base::TimeDelta::FromDays(1)));

  // And another half seconds worth.
  rate_limiter.add_samples(500);
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1500),
            rate_limiter.ComputeTimeNeeded());
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(1500),
            rate_limiter.ComputeDelayNeeded(base::TimeDelta()));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(500),
            rate_limiter.ComputeDelayNeeded(base::TimeDelta::FromSeconds(1)));
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(750),
            rate_limiter.ComputeDelayNeeded(
                base::TimeDelta::FromMilliseconds(750)));
  EXPECT_EQ(base::TimeDelta(),
            rate_limiter.ComputeDelayNeeded(
                base::TimeDelta::FromMilliseconds(1500)));
  EXPECT_EQ(base::TimeDelta(),
            rate_limiter.ComputeDelayNeeded(
                base::TimeDelta::FromDays(1)));
}

}  // namespace content
