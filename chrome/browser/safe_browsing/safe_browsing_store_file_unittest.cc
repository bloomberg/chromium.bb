// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_store_file.h"
#include "chrome/browser/safe_browsing/safe_browsing_store_sqlite.h"

#include "base/callback.h"
#include "chrome/browser/safe_browsing/safe_browsing_store_unittest_helper.h"
#include "chrome/test/file_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

const FilePath::CharType kFolderPrefix[] =
    FILE_PATH_LITERAL("SafeBrowsingTestStoreFile");

class SafeBrowsingStoreFileTest : public PlatformTest {
 public:
  virtual void SetUp() {
    PlatformTest::SetUp();

    FilePath temp_dir;
    ASSERT_TRUE(file_util::CreateNewTempDirectory(kFolderPrefix, &temp_dir));

    file_deleter_.reset(new FileAutoDeleter(temp_dir));

    filename_ = temp_dir;
    filename_ = filename_.AppendASCII("SafeBrowsingTestStore");
    file_util::Delete(filename_, false);

    // Make sure an old temporary file isn't hanging around.
    const FilePath temp_file =
        SafeBrowsingStoreFile::TemporaryFileForFilename(filename_);
    file_util::Delete(temp_file, false);

    store_.reset(new SafeBrowsingStoreFile());
    store_->Init(filename_, NULL);
  }
  virtual void TearDown() {
    if (store_.get())
      store_->Delete();
    store_.reset();
    file_deleter_.reset();

    PlatformTest::TearDown();
  }

  void OnCorruptionDetected() {
    corruption_detected_ = true;
  }

  scoped_ptr<FileAutoDeleter> file_deleter_;
  FilePath filename_;
  scoped_ptr<SafeBrowsingStoreFile> store_;
  bool corruption_detected_;
};

TEST_STORE(SafeBrowsingStoreFileTest, store_.get(), filename_);

// Test that Delete() deletes the temporary store, if present.
TEST_F(SafeBrowsingStoreFileTest, DeleteTemp) {
    const FilePath temp_file =
        SafeBrowsingStoreFile::TemporaryFileForFilename(filename_);

  EXPECT_FALSE(file_util::PathExists(filename_));
  EXPECT_FALSE(file_util::PathExists(temp_file));

  // Starting a transaction creates a temporary file.
  EXPECT_TRUE(store_->BeginUpdate());
  EXPECT_TRUE(file_util::PathExists(temp_file));

  // Pull the rug out from under the existing store, simulating a
  // crash.
  store_.reset(new SafeBrowsingStoreFile());
  store_->Init(filename_, NULL);
  EXPECT_FALSE(file_util::PathExists(filename_));
  EXPECT_TRUE(file_util::PathExists(temp_file));

  // Make sure the temporary file is deleted.
  EXPECT_TRUE(store_->Delete());
  EXPECT_FALSE(file_util::PathExists(filename_));
  EXPECT_FALSE(file_util::PathExists(temp_file));
}

// Test basic corruption-handling.
TEST_F(SafeBrowsingStoreFileTest, DetectsCorruption) {
  // Load a store with some data.
  SafeBrowsingStoreTestStorePrefix(store_.get());

  SafeBrowsingStoreFile test_store;
  test_store.Init(
      filename_,
      NewCallback(static_cast<SafeBrowsingStoreFileTest*>(this),
                  &SafeBrowsingStoreFileTest::OnCorruptionDetected));

  corruption_detected_ = false;

  // Can successfully open and read the store.
  std::vector<SBAddFullHash> pending_adds;
  std::vector<SBAddPrefix> orig_prefixes;
  std::vector<SBAddFullHash> orig_hashes;
  EXPECT_TRUE(test_store.BeginUpdate());
  EXPECT_TRUE(test_store.FinishUpdate(pending_adds,
                                      &orig_prefixes, &orig_hashes));
  EXPECT_GT(orig_prefixes.size(), 0U);
  EXPECT_GT(orig_hashes.size(), 0U);
  EXPECT_FALSE(corruption_detected_);

  // Corrupt the store.
  file_util::ScopedFILE file(file_util::OpenFile(filename_, "rb+"));
  const long kOffset = 60;
  EXPECT_EQ(fseek(file.get(), kOffset, SEEK_SET), 0);
  const int32 kZero = 0;
  int32 previous = kZero;
  EXPECT_EQ(fread(&previous, sizeof(previous), 1, file.get()), 1U);
  EXPECT_NE(previous, kZero);
  EXPECT_EQ(fseek(file.get(), kOffset, SEEK_SET), 0);
  EXPECT_EQ(fwrite(&kZero, sizeof(kZero), 1, file.get()), 1U);
  file.reset();

  // Update fails and corruption callback is called.
  std::vector<SBAddPrefix> add_prefixes;
  std::vector<SBAddFullHash> add_hashes;
  corruption_detected_ = false;
  EXPECT_TRUE(test_store.BeginUpdate());
  EXPECT_FALSE(test_store.FinishUpdate(pending_adds,
                                       &add_prefixes, &add_hashes));
  EXPECT_TRUE(corruption_detected_);
  EXPECT_EQ(add_prefixes.size(), 0U);
  EXPECT_EQ(add_hashes.size(), 0U);

  // Make it look like there is a lot of add-chunks-seen data.
  const long kAddChunkCountOffset = 2 * sizeof(int32);
  const int32 kLargeCount = 1000 * 1000 * 1000;
  file.reset(file_util::OpenFile(filename_, "rb+"));
  EXPECT_EQ(fseek(file.get(), kAddChunkCountOffset, SEEK_SET), 0);
  EXPECT_EQ(fwrite(&kLargeCount, sizeof(kLargeCount), 1, file.get()), 1U);
  file.reset();

  // Detects corruption and fails to even begin the update.
  corruption_detected_ = false;
  EXPECT_FALSE(test_store.BeginUpdate());
  EXPECT_TRUE(corruption_detected_);
}

// Info to build a trivial store for migration testing.
const int kAddChunk1 = 1;
const int kAddChunk2 = 3;
const int kAddChunk3 = 5;
const int kSubChunk1 = 2;
const int kSubChunk2 = 4;
const SBFullHash kHash1 = SBFullHashFromString("one");
const SBFullHash kHash2 = SBFullHashFromString("two");
const SBPrefix kPrefix1 = kHash1.prefix;
const SBPrefix kPrefix2 = kHash2.prefix;
const SBPrefix kPrefix3 = SBFullHashFromString("three").prefix;
const SBPrefix kPrefix4 = SBFullHashFromString("four").prefix;
const SBPrefix kPrefix5 = SBFullHashFromString("five").prefix;

// Load the store with some data.
void LoadStore(SafeBrowsingStore* store) {
  EXPECT_TRUE(store->BeginUpdate());
  EXPECT_TRUE(store->BeginChunk());

  store->SetAddChunk(kAddChunk1);
  store->SetSubChunk(kSubChunk1);
  store->SetAddChunk(kAddChunk2);

  EXPECT_TRUE(store->WriteAddPrefix(kAddChunk1, kPrefix1));
  EXPECT_TRUE(store->WriteAddPrefix(kAddChunk1, kPrefix2));
  EXPECT_TRUE(store->WriteAddPrefix(kAddChunk2, kPrefix3));
  EXPECT_TRUE(store->WriteSubPrefix(kSubChunk1, kAddChunk3, kPrefix4));
  EXPECT_TRUE(store->WriteAddHash(kAddChunk1, base::Time::Now(), kHash1));
  EXPECT_TRUE(store->WriteSubHash(kSubChunk1, kAddChunk1, kHash2));

  EXPECT_TRUE(store->FinishChunk());

  std::vector<SBAddFullHash> pending_adds;
  std::vector<SBAddPrefix> add_prefixes;
  std::vector<SBAddFullHash> add_hashes;
  EXPECT_TRUE(store->FinishUpdate(pending_adds, &add_prefixes, &add_hashes));
  EXPECT_EQ(3U, add_prefixes.size());
}

// Verify that the store looks like what results from LoadStore(), and
// update it.
void UpdateStore(SafeBrowsingStore* store) {
  EXPECT_TRUE(store->BeginUpdate());
  EXPECT_TRUE(store->BeginChunk());

  // The chunks in the database should be the same.
  std::vector<int> add_chunks;
  store->GetAddChunks(&add_chunks);
  ASSERT_EQ(2U, add_chunks.size());
  EXPECT_EQ(kAddChunk1, add_chunks[0]);
  EXPECT_EQ(kAddChunk2, add_chunks[1]);

  std::vector<int> sub_chunks;
  store->GetSubChunks(&sub_chunks);
  ASSERT_EQ(1U, sub_chunks.size());
  EXPECT_EQ(kSubChunk1, sub_chunks[0]);

  EXPECT_TRUE(store->CheckAddChunk(kAddChunk1));
  EXPECT_TRUE(store->CheckSubChunk(kSubChunk1));
  EXPECT_TRUE(store->CheckAddChunk(kAddChunk2));

  EXPECT_FALSE(store->CheckAddChunk(kAddChunk3));
  store->SetAddChunk(kAddChunk3);
  // This one already has a sub.
  EXPECT_TRUE(store->WriteAddPrefix(kAddChunk3, kPrefix4));
  EXPECT_TRUE(store->WriteAddPrefix(kAddChunk3, kPrefix5));

  EXPECT_FALSE(store->CheckSubChunk(kSubChunk2));
  store->SetSubChunk(kSubChunk2);
  EXPECT_TRUE(store->WriteSubPrefix(kSubChunk2, kAddChunk1, kPrefix1));

  EXPECT_TRUE(store->FinishChunk());

  store->DeleteAddChunk(kAddChunk2);

  std::vector<SBAddFullHash> pending_adds;
  std::vector<SBAddPrefix> add_prefixes;
  std::vector<SBAddFullHash> add_hashes;
  EXPECT_TRUE(store->FinishUpdate(pending_adds, &add_prefixes, &add_hashes));
  EXPECT_EQ(2U, add_prefixes.size());
}

// Verify that the expected UpdateStore() data is present.
void CheckStore(SafeBrowsingStore* store) {
  EXPECT_TRUE(store->BeginUpdate());

  // The chunks in the database should be the same.
  std::vector<int> add_chunks;
  store->GetAddChunks(&add_chunks);
  ASSERT_EQ(2U, add_chunks.size());
  EXPECT_EQ(kAddChunk1, add_chunks[0]);
  EXPECT_EQ(kAddChunk3, add_chunks[1]);

  std::vector<int> sub_chunks;
  store->GetSubChunks(&sub_chunks);
  ASSERT_EQ(2U, sub_chunks.size());
  EXPECT_EQ(kSubChunk1, sub_chunks[0]);
  EXPECT_EQ(kSubChunk2, sub_chunks[1]);

  EXPECT_TRUE(store->CancelUpdate());
}

// Verify that the migration sequence works as expected in the
// non-migration cases.
TEST_F(SafeBrowsingStoreFileTest, MigrateBaselineFile) {
  LoadStore(store_.get());
  UpdateStore(store_.get());
  CheckStore(store_.get());
}
TEST_F(SafeBrowsingStoreFileTest, MigrateBaselineSqlite) {
  SafeBrowsingStoreSqlite sqlite_store;
  sqlite_store.Init(filename_, NULL);

  LoadStore(&sqlite_store);
  UpdateStore(&sqlite_store);
  CheckStore(&sqlite_store);
}

// The sequence should work exactly the same when we migrate from
// SQLite to file.
TEST_F(SafeBrowsingStoreFileTest, Migrate) {
  // No existing store.
  EXPECT_FALSE(file_util::PathExists(filename_));

  {
    SafeBrowsingStoreSqlite sqlite_store;
    sqlite_store.Init(filename_, NULL);

    LoadStore(&sqlite_store);
  }

  // At this point |filename_| references a SQLite store.
  EXPECT_TRUE(file_util::PathExists(filename_));

  // Update and check using a file store.
  UpdateStore(store_.get());
  CheckStore(store_.get());
}

}  // namespace
