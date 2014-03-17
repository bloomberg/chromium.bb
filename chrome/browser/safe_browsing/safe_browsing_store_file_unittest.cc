// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_store_file.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/md5.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

const int kAddChunk1 = 1;
const int kAddChunk2 = 3;
const int kAddChunk3 = 5;
const int kAddChunk4 = 7;
// Disjoint chunk numbers for subs to flush out typos.
const int kSubChunk1 = 2;
const int kSubChunk2 = 4;

const SBFullHash kHash1 = SBFullHashForString("one");
const SBFullHash kHash2 = SBFullHashForString("two");
const SBFullHash kHash3 = SBFullHashForString("three");
const SBFullHash kHash4 = SBFullHashForString("four");
const SBFullHash kHash5 = SBFullHashForString("five");

class SafeBrowsingStoreFileTest : public PlatformTest {
 public:
  virtual void SetUp() {
    PlatformTest::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    filename_ = temp_dir_.path();
    filename_ = filename_.AppendASCII("SafeBrowsingTestStore");

    store_.reset(new SafeBrowsingStoreFile());
    store_->Init(filename_,
                 base::Bind(&SafeBrowsingStoreFileTest::OnCorruptionDetected,
                            base::Unretained(this)));
    corruption_detected_ = false;
  }
  virtual void TearDown() {
    if (store_.get())
      store_->Delete();
    store_.reset();

    PlatformTest::TearDown();
  }

  void OnCorruptionDetected() {
    corruption_detected_ = true;
  }

  // Populate the store with some testing data.
  void PopulateStore(const base::Time& now) {
    EXPECT_TRUE(store_->BeginUpdate());

    EXPECT_TRUE(store_->BeginChunk());
    store_->SetAddChunk(kAddChunk1);
    EXPECT_TRUE(store_->CheckAddChunk(kAddChunk1));
    EXPECT_TRUE(store_->WriteAddPrefix(kAddChunk1, kHash1.prefix));
    EXPECT_TRUE(store_->WriteAddPrefix(kAddChunk1, kHash2.prefix));
    EXPECT_TRUE(store_->WriteAddHash(kAddChunk1, now, kHash2));

    store_->SetSubChunk(kSubChunk1);
    EXPECT_TRUE(store_->CheckSubChunk(kSubChunk1));
    EXPECT_TRUE(store_->WriteSubPrefix(kSubChunk1, kAddChunk3, kHash3.prefix));
    EXPECT_TRUE(store_->WriteSubHash(kSubChunk1, kAddChunk3, kHash3));
    EXPECT_TRUE(store_->FinishChunk());

    // Chunk numbers shouldn't leak over.
    EXPECT_FALSE(store_->CheckAddChunk(kSubChunk1));
    EXPECT_FALSE(store_->CheckAddChunk(kAddChunk3));
    EXPECT_FALSE(store_->CheckSubChunk(kAddChunk1));

    std::vector<SBAddFullHash> pending_adds;
    SBAddPrefixes add_prefixes_result;
    std::vector<SBAddFullHash> add_full_hashes_result;

    EXPECT_TRUE(store_->FinishUpdate(pending_adds,
                                     &add_prefixes_result,
                                     &add_full_hashes_result));
}

  base::ScopedTempDir temp_dir_;
  base::FilePath filename_;
  scoped_ptr<SafeBrowsingStoreFile> store_;
  bool corruption_detected_;
};

// Test that the empty store looks empty.
TEST_F(SafeBrowsingStoreFileTest, Empty) {
  EXPECT_TRUE(store_->BeginUpdate());

  std::vector<int> chunks;
  store_->GetAddChunks(&chunks);
  EXPECT_TRUE(chunks.empty());
  store_->GetSubChunks(&chunks);
  EXPECT_TRUE(chunks.empty());

  // Shouldn't see anything, but anything is a big set to test.
  EXPECT_FALSE(store_->CheckAddChunk(0));
  EXPECT_FALSE(store_->CheckAddChunk(1));
  EXPECT_FALSE(store_->CheckAddChunk(-1));

  EXPECT_FALSE(store_->CheckSubChunk(0));
  EXPECT_FALSE(store_->CheckSubChunk(1));
  EXPECT_FALSE(store_->CheckSubChunk(-1));

  std::vector<SBAddFullHash> pending_adds;
  SBAddPrefixes add_prefixes_result;
  std::vector<SBAddFullHash> add_full_hashes_result;

  EXPECT_TRUE(store_->FinishUpdate(pending_adds,
                                   &add_prefixes_result,
                                   &add_full_hashes_result));
  EXPECT_TRUE(add_prefixes_result.empty());
  EXPECT_TRUE(add_full_hashes_result.empty());
}

// Write some prefix data to the store and verify that it looks like
// it is still there after the transaction completes.
TEST_F(SafeBrowsingStoreFileTest, StorePrefix) {
  const base::Time now = base::Time::Now();
  PopulateStore(now);

  EXPECT_TRUE(store_->BeginUpdate());

  std::vector<int> chunks;
  store_->GetAddChunks(&chunks);
  ASSERT_EQ(1U, chunks.size());
  EXPECT_EQ(kAddChunk1, chunks[0]);

  store_->GetSubChunks(&chunks);
  ASSERT_EQ(1U, chunks.size());
  EXPECT_EQ(kSubChunk1, chunks[0]);

  std::vector<SBAddFullHash> pending_adds;
  SBAddPrefixes add_prefixes_result;
  std::vector<SBAddFullHash> add_full_hashes_result;

  EXPECT_TRUE(store_->FinishUpdate(pending_adds,
                                   &add_prefixes_result,
                                   &add_full_hashes_result));

  ASSERT_EQ(2U, add_prefixes_result.size());
  EXPECT_EQ(kAddChunk1, add_prefixes_result[0].chunk_id);
  EXPECT_EQ(kHash1.prefix, add_prefixes_result[0].prefix);
  EXPECT_EQ(kAddChunk1, add_prefixes_result[1].chunk_id);
  EXPECT_EQ(kHash2.prefix, add_prefixes_result[1].prefix);

  ASSERT_EQ(1U, add_full_hashes_result.size());
  EXPECT_EQ(kAddChunk1, add_full_hashes_result[0].chunk_id);
  // EXPECT_TRUE(add_full_hashes_result[0].received == now)?
  EXPECT_EQ(now.ToTimeT(), add_full_hashes_result[0].received);
  EXPECT_TRUE(SBFullHashEqual(kHash2, add_full_hashes_result[0].full_hash));

  add_prefixes_result.clear();
  add_full_hashes_result.clear();

  EXPECT_TRUE(store_->BeginUpdate());

  // Still has the chunks expected in the next update.
  store_->GetAddChunks(&chunks);
  ASSERT_EQ(1U, chunks.size());
  EXPECT_EQ(kAddChunk1, chunks[0]);

  store_->GetSubChunks(&chunks);
  ASSERT_EQ(1U, chunks.size());
  EXPECT_EQ(kSubChunk1, chunks[0]);

  EXPECT_TRUE(store_->CheckAddChunk(kAddChunk1));
  EXPECT_TRUE(store_->CheckSubChunk(kSubChunk1));

  EXPECT_TRUE(store_->FinishUpdate(pending_adds,
                                   &add_prefixes_result,
                                   &add_full_hashes_result));

  // Still has the expected contents.
  ASSERT_EQ(2U, add_prefixes_result.size());
  EXPECT_EQ(kAddChunk1, add_prefixes_result[0].chunk_id);
  EXPECT_EQ(kHash1.prefix, add_prefixes_result[0].prefix);
  EXPECT_EQ(kAddChunk1, add_prefixes_result[1].chunk_id);
  EXPECT_EQ(kHash2.prefix, add_prefixes_result[1].prefix);

  ASSERT_EQ(1U, add_full_hashes_result.size());
  EXPECT_EQ(kAddChunk1, add_full_hashes_result[0].chunk_id);
  EXPECT_EQ(now.ToTimeT(), add_full_hashes_result[0].received);
  EXPECT_TRUE(SBFullHashEqual(kHash2, add_full_hashes_result[0].full_hash));
}

// Test that subs knockout adds.
TEST_F(SafeBrowsingStoreFileTest, SubKnockout) {
  EXPECT_TRUE(store_->BeginUpdate());

  const base::Time now = base::Time::Now();

  EXPECT_TRUE(store_->BeginChunk());
  store_->SetAddChunk(kAddChunk1);
  EXPECT_TRUE(store_->WriteAddPrefix(kAddChunk1, kHash1.prefix));
  EXPECT_TRUE(store_->WriteAddPrefix(kAddChunk1, kHash2.prefix));
  EXPECT_TRUE(store_->WriteAddHash(kAddChunk1, now, kHash2));

  store_->SetSubChunk(kSubChunk1);
  EXPECT_TRUE(store_->WriteSubPrefix(kSubChunk1, kAddChunk3, kHash3.prefix));
  EXPECT_TRUE(store_->WriteSubPrefix(kSubChunk1, kAddChunk1, kHash2.prefix));
  EXPECT_TRUE(store_->FinishChunk());

  std::vector<SBAddFullHash> pending_adds;
  SBAddPrefixes add_prefixes_result;
  std::vector<SBAddFullHash> add_full_hashes_result;

  EXPECT_TRUE(store_->FinishUpdate(pending_adds,
                                   &add_prefixes_result,
                                   &add_full_hashes_result));

  // Knocked out the chunk expected.
  ASSERT_EQ(1U, add_prefixes_result.size());
  EXPECT_EQ(kAddChunk1, add_prefixes_result[0].chunk_id);
  EXPECT_EQ(kHash1.prefix, add_prefixes_result[0].prefix);
  EXPECT_TRUE(add_full_hashes_result.empty());

  add_prefixes_result.clear();

  EXPECT_TRUE(store_->BeginUpdate());

  // This add should be knocked out by an existing sub.
  EXPECT_TRUE(store_->BeginChunk());
  store_->SetAddChunk(kAddChunk3);
  EXPECT_TRUE(store_->WriteAddPrefix(kAddChunk3, kHash3.prefix));
  EXPECT_TRUE(store_->FinishChunk());

  EXPECT_TRUE(store_->FinishUpdate(pending_adds,
                                   &add_prefixes_result,
                                   &add_full_hashes_result));
  EXPECT_EQ(1U, add_prefixes_result.size());
  EXPECT_EQ(kAddChunk1, add_prefixes_result[0].chunk_id);
  EXPECT_EQ(kHash1.prefix, add_prefixes_result[0].prefix);
  EXPECT_TRUE(add_full_hashes_result.empty());

  add_prefixes_result.clear();

  EXPECT_TRUE(store_->BeginUpdate());

  // But by here the sub should be gone, so it should stick this time.
  EXPECT_TRUE(store_->BeginChunk());
  store_->SetAddChunk(kAddChunk3);
  EXPECT_TRUE(store_->WriteAddPrefix(kAddChunk3, kHash3.prefix));
  EXPECT_TRUE(store_->FinishChunk());

  EXPECT_TRUE(store_->FinishUpdate(pending_adds,
                                   &add_prefixes_result,
                                   &add_full_hashes_result));
  ASSERT_EQ(2U, add_prefixes_result.size());
  EXPECT_EQ(kAddChunk1, add_prefixes_result[0].chunk_id);
  EXPECT_EQ(kHash1.prefix, add_prefixes_result[0].prefix);
  EXPECT_EQ(kAddChunk3, add_prefixes_result[1].chunk_id);
  EXPECT_EQ(kHash3.prefix, add_prefixes_result[1].prefix);
  EXPECT_TRUE(add_full_hashes_result.empty());
}

// Test that deletes delete the chunk's data.
TEST_F(SafeBrowsingStoreFileTest, DeleteChunks) {
  EXPECT_TRUE(store_->BeginUpdate());

  const base::Time now = base::Time::Now();

  // A chunk which will be deleted.
  EXPECT_FALSE(store_->CheckAddChunk(kAddChunk1));
  store_->SetAddChunk(kAddChunk1);
  EXPECT_TRUE(store_->BeginChunk());
  EXPECT_TRUE(store_->WriteAddPrefix(kAddChunk1, kHash1.prefix));
  EXPECT_TRUE(store_->WriteAddPrefix(kAddChunk1, kHash2.prefix));
  EXPECT_TRUE(store_->WriteAddHash(kAddChunk1, now, kHash2));
  EXPECT_TRUE(store_->FinishChunk());

  // Another which won't.
  EXPECT_FALSE(store_->CheckAddChunk(kAddChunk2));
  store_->SetAddChunk(kAddChunk2);
  EXPECT_TRUE(store_->BeginChunk());
  EXPECT_TRUE(store_->WriteAddPrefix(kAddChunk2, kHash3.prefix));
  EXPECT_TRUE(store_->WriteAddHash(kAddChunk2, now, kHash3));
  EXPECT_TRUE(store_->FinishChunk());

  // A sub chunk to delete.
  EXPECT_FALSE(store_->CheckSubChunk(kSubChunk1));
  store_->SetSubChunk(kSubChunk1);
  EXPECT_TRUE(store_->BeginChunk());
  EXPECT_TRUE(store_->WriteSubPrefix(kSubChunk1, kAddChunk3, kHash4.prefix));
  EXPECT_TRUE(store_->WriteSubHash(kSubChunk1, kAddChunk3, kHash4));
  EXPECT_TRUE(store_->FinishChunk());

  // A sub chunk to keep.
  EXPECT_FALSE(store_->CheckSubChunk(kSubChunk2));
  store_->SetSubChunk(kSubChunk2);
  EXPECT_TRUE(store_->BeginChunk());
  EXPECT_TRUE(store_->WriteSubPrefix(kSubChunk2, kAddChunk4, kHash5.prefix));
  EXPECT_TRUE(store_->WriteSubHash(kSubChunk2, kAddChunk4, kHash5));
  EXPECT_TRUE(store_->FinishChunk());

  store_->DeleteAddChunk(kAddChunk1);
  store_->DeleteSubChunk(kSubChunk1);

  // Not actually deleted until finish.
  EXPECT_TRUE(store_->CheckAddChunk(kAddChunk1));
  EXPECT_TRUE(store_->CheckAddChunk(kAddChunk2));
  EXPECT_TRUE(store_->CheckSubChunk(kSubChunk1));
  EXPECT_TRUE(store_->CheckSubChunk(kSubChunk2));

  std::vector<SBAddFullHash> pending_adds;
  SBAddPrefixes add_prefixes_result;
  std::vector<SBAddFullHash> add_full_hashes_result;

  EXPECT_TRUE(store_->FinishUpdate(pending_adds,
                                   &add_prefixes_result,
                                   &add_full_hashes_result));

  EXPECT_EQ(1U, add_prefixes_result.size());
  EXPECT_EQ(kAddChunk2, add_prefixes_result[0].chunk_id);
  EXPECT_EQ(kHash3.prefix, add_prefixes_result[0].prefix);
  EXPECT_EQ(1U, add_full_hashes_result.size());
  EXPECT_EQ(kAddChunk2, add_full_hashes_result[0].chunk_id);
  EXPECT_EQ(now.ToTimeT(), add_full_hashes_result[0].received);
  EXPECT_TRUE(SBFullHashEqual(kHash3, add_full_hashes_result[0].full_hash));

  // Expected chunks are there in another update.
  EXPECT_TRUE(store_->BeginUpdate());
  EXPECT_FALSE(store_->CheckAddChunk(kAddChunk1));
  EXPECT_TRUE(store_->CheckAddChunk(kAddChunk2));
  EXPECT_FALSE(store_->CheckSubChunk(kSubChunk1));
  EXPECT_TRUE(store_->CheckSubChunk(kSubChunk2));

  // Delete them, too.
  store_->DeleteAddChunk(kAddChunk2);
  store_->DeleteSubChunk(kSubChunk2);

  add_prefixes_result.clear();
  add_full_hashes_result.clear();
  EXPECT_TRUE(store_->FinishUpdate(pending_adds,
                                   &add_prefixes_result,
                                   &add_full_hashes_result));

  // Expect no more chunks.
  EXPECT_TRUE(store_->BeginUpdate());
  EXPECT_FALSE(store_->CheckAddChunk(kAddChunk1));
  EXPECT_FALSE(store_->CheckAddChunk(kAddChunk2));
  EXPECT_FALSE(store_->CheckSubChunk(kSubChunk1));
  EXPECT_FALSE(store_->CheckSubChunk(kSubChunk2));
  add_prefixes_result.clear();
  add_full_hashes_result.clear();
  EXPECT_TRUE(store_->FinishUpdate(pending_adds,
                                   &add_prefixes_result,
                                   &add_full_hashes_result));
  EXPECT_TRUE(add_prefixes_result.empty());
  EXPECT_TRUE(add_full_hashes_result.empty());
}

// Test that deleting the store deletes the store.
TEST_F(SafeBrowsingStoreFileTest, Delete) {
  // Delete should work if the file wasn't there in the first place.
  EXPECT_FALSE(base::PathExists(filename_));
  EXPECT_TRUE(store_->Delete());

  // Create a store file.
  PopulateStore(base::Time::Now());

  EXPECT_TRUE(base::PathExists(filename_));
  EXPECT_TRUE(store_->Delete());
  EXPECT_FALSE(base::PathExists(filename_));
}

// Test that Delete() deletes the temporary store, if present.
TEST_F(SafeBrowsingStoreFileTest, DeleteTemp) {
  const base::FilePath temp_file =
      SafeBrowsingStoreFile::TemporaryFileForFilename(filename_);

  EXPECT_FALSE(base::PathExists(filename_));
  EXPECT_FALSE(base::PathExists(temp_file));

  // Starting a transaction creates a temporary file.
  EXPECT_TRUE(store_->BeginUpdate());
  EXPECT_TRUE(base::PathExists(temp_file));

  // Pull the rug out from under the existing store, simulating a
  // crash.
  store_.reset(new SafeBrowsingStoreFile());
  store_->Init(filename_, base::Closure());
  EXPECT_FALSE(base::PathExists(filename_));
  EXPECT_TRUE(base::PathExists(temp_file));

  // Make sure the temporary file is deleted.
  EXPECT_TRUE(store_->Delete());
  EXPECT_FALSE(base::PathExists(filename_));
  EXPECT_FALSE(base::PathExists(temp_file));
}

// Test basic corruption-handling.
TEST_F(SafeBrowsingStoreFileTest, DetectsCorruption) {
  // Load a store with some data.
  PopulateStore(base::Time::Now());

  // Can successfully open and read the store.
  std::vector<SBAddFullHash> pending_adds;
  SBAddPrefixes orig_prefixes;
  std::vector<SBAddFullHash> orig_hashes;
  EXPECT_TRUE(store_->BeginUpdate());
  EXPECT_TRUE(store_->FinishUpdate(pending_adds, &orig_prefixes, &orig_hashes));
  EXPECT_GT(orig_prefixes.size(), 0U);
  EXPECT_GT(orig_hashes.size(), 0U);
  EXPECT_FALSE(corruption_detected_);

  // Corrupt the store.
  base::ScopedFILE file(base::OpenFile(filename_, "rb+"));
  const long kOffset = 60;
  EXPECT_EQ(fseek(file.get(), kOffset, SEEK_SET), 0);
  const uint32 kZero = 0;
  uint32 previous = kZero;
  EXPECT_EQ(fread(&previous, sizeof(previous), 1, file.get()), 1U);
  EXPECT_NE(previous, kZero);
  EXPECT_EQ(fseek(file.get(), kOffset, SEEK_SET), 0);
  EXPECT_EQ(fwrite(&kZero, sizeof(kZero), 1, file.get()), 1U);
  file.reset();

  // Update fails and corruption callback is called.
  SBAddPrefixes add_prefixes;
  std::vector<SBAddFullHash> add_hashes;
  corruption_detected_ = false;
  EXPECT_TRUE(store_->BeginUpdate());
  EXPECT_FALSE(store_->FinishUpdate(pending_adds, &add_prefixes, &add_hashes));
  EXPECT_TRUE(corruption_detected_);
  EXPECT_EQ(add_prefixes.size(), 0U);
  EXPECT_EQ(add_hashes.size(), 0U);

  // Make it look like there is a lot of add-chunks-seen data.
  const long kAddChunkCountOffset = 2 * sizeof(int32);
  const int32 kLargeCount = 1000 * 1000 * 1000;
  file.reset(base::OpenFile(filename_, "rb+"));
  EXPECT_EQ(fseek(file.get(), kAddChunkCountOffset, SEEK_SET), 0);
  EXPECT_EQ(fwrite(&kLargeCount, sizeof(kLargeCount), 1, file.get()), 1U);
  file.reset();

  // Detects corruption and fails to even begin the update.
  corruption_detected_ = false;
  EXPECT_FALSE(store_->BeginUpdate());
  EXPECT_TRUE(corruption_detected_);
}

TEST_F(SafeBrowsingStoreFileTest, CheckValidity) {
  // Empty store is valid.
  EXPECT_FALSE(base::PathExists(filename_));
  ASSERT_TRUE(store_->BeginUpdate());
  EXPECT_FALSE(corruption_detected_);
  EXPECT_TRUE(store_->CheckValidity());
  EXPECT_FALSE(corruption_detected_);
  EXPECT_TRUE(store_->CancelUpdate());

  // A store with some data is valid.
  EXPECT_FALSE(base::PathExists(filename_));
  PopulateStore(base::Time::Now());
  EXPECT_TRUE(base::PathExists(filename_));
  ASSERT_TRUE(store_->BeginUpdate());
  EXPECT_FALSE(corruption_detected_);
  EXPECT_TRUE(store_->CheckValidity());
  EXPECT_FALSE(corruption_detected_);
  EXPECT_TRUE(store_->CancelUpdate());
}

// Corrupt the payload.
TEST_F(SafeBrowsingStoreFileTest, CheckValidityPayload) {
  PopulateStore(base::Time::Now());
  EXPECT_TRUE(base::PathExists(filename_));

  // 37 is the most random prime number.  It's also past the header,
  // as corrupting the header would fail BeginUpdate() in which case
  // CheckValidity() cannot be called.
  const size_t kOffset = 37;

  {
    base::ScopedFILE file(base::OpenFile(filename_, "rb+"));
    EXPECT_EQ(0, fseek(file.get(), kOffset, SEEK_SET));
    EXPECT_GE(fputs("hello", file.get()), 0);
  }
  ASSERT_TRUE(store_->BeginUpdate());
  EXPECT_FALSE(corruption_detected_);
  EXPECT_FALSE(store_->CheckValidity());
  EXPECT_TRUE(corruption_detected_);
  EXPECT_TRUE(store_->CancelUpdate());
}

// Corrupt the checksum.
TEST_F(SafeBrowsingStoreFileTest, CheckValidityChecksum) {
  PopulateStore(base::Time::Now());
  EXPECT_TRUE(base::PathExists(filename_));

  // An offset from the end of the file which is in the checksum.
  const int kOffset = -static_cast<int>(sizeof(base::MD5Digest));

  {
    base::ScopedFILE file(base::OpenFile(filename_, "rb+"));
    EXPECT_EQ(0, fseek(file.get(), kOffset, SEEK_END));
    EXPECT_GE(fputs("hello", file.get()), 0);
  }
  ASSERT_TRUE(store_->BeginUpdate());
  EXPECT_FALSE(corruption_detected_);
  EXPECT_FALSE(store_->CheckValidity());
  EXPECT_TRUE(corruption_detected_);
  EXPECT_TRUE(store_->CancelUpdate());
}

}  // namespace
