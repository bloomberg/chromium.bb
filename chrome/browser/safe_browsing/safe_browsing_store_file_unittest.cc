// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_store_file.h"

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/md5.h"
#include "chrome/browser/safe_browsing/safe_browsing_store_unittest_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

const base::FilePath::CharType kFolderPrefix[] =
    FILE_PATH_LITERAL("SafeBrowsingTestStoreFile");

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

  base::ScopedTempDir temp_dir_;
  base::FilePath filename_;
  scoped_ptr<SafeBrowsingStoreFile> store_;
  bool corruption_detected_;
};

TEST_STORE(SafeBrowsingStoreFileTest, store_.get(), filename_);

// Test that Delete() deletes the temporary store, if present.
TEST_F(SafeBrowsingStoreFileTest, DeleteTemp) {
  const base::FilePath temp_file =
      SafeBrowsingStoreFile::TemporaryFileForFilename(filename_);

  EXPECT_FALSE(file_util::PathExists(filename_));
  EXPECT_FALSE(file_util::PathExists(temp_file));

  // Starting a transaction creates a temporary file.
  EXPECT_TRUE(store_->BeginUpdate());
  EXPECT_TRUE(file_util::PathExists(temp_file));

  // Pull the rug out from under the existing store, simulating a
  // crash.
  store_.reset(new SafeBrowsingStoreFile());
  store_->Init(filename_, base::Closure());
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

  // Can successfully open and read the store.
  std::vector<SBAddFullHash> pending_adds;
  std::set<SBPrefix> prefix_misses;
  SBAddPrefixes orig_prefixes;
  std::vector<SBAddFullHash> orig_hashes;
  EXPECT_TRUE(store_->BeginUpdate());
  EXPECT_TRUE(store_->FinishUpdate(pending_adds, prefix_misses,
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
  SBAddPrefixes add_prefixes;
  std::vector<SBAddFullHash> add_hashes;
  corruption_detected_ = false;
  EXPECT_TRUE(store_->BeginUpdate());
  EXPECT_FALSE(store_->FinishUpdate(pending_adds, prefix_misses,
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
  EXPECT_FALSE(store_->BeginUpdate());
  EXPECT_TRUE(corruption_detected_);
}

TEST_F(SafeBrowsingStoreFileTest, CheckValidity) {
  // Empty store is valid.
  EXPECT_FALSE(file_util::PathExists(filename_));
  ASSERT_TRUE(store_->BeginUpdate());
  EXPECT_FALSE(corruption_detected_);
  EXPECT_TRUE(store_->CheckValidity());
  EXPECT_FALSE(corruption_detected_);
  EXPECT_TRUE(store_->CancelUpdate());

  // A store with some data is valid.
  EXPECT_FALSE(file_util::PathExists(filename_));
  SafeBrowsingStoreTestStorePrefix(store_.get());
  EXPECT_TRUE(file_util::PathExists(filename_));
  ASSERT_TRUE(store_->BeginUpdate());
  EXPECT_FALSE(corruption_detected_);
  EXPECT_TRUE(store_->CheckValidity());
  EXPECT_FALSE(corruption_detected_);
  EXPECT_TRUE(store_->CancelUpdate());
}

// Corrupt the payload.
TEST_F(SafeBrowsingStoreFileTest, CheckValidityPayload) {
  SafeBrowsingStoreTestStorePrefix(store_.get());
  EXPECT_TRUE(file_util::PathExists(filename_));

  // 37 is the most random prime number.  It's also past the header,
  // as corrupting the header would fail BeginUpdate() in which case
  // CheckValidity() cannot be called.
  const size_t kOffset = 37;

  {
    file_util::ScopedFILE file(file_util::OpenFile(filename_, "rb+"));
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
  SafeBrowsingStoreTestStorePrefix(store_.get());
  EXPECT_TRUE(file_util::PathExists(filename_));

  // An offset from the end of the file which is in the checksum.
  const int kOffset = -static_cast<int>(sizeof(base::MD5Digest));

  {
    file_util::ScopedFILE file(file_util::OpenFile(filename_, "rb+"));
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
