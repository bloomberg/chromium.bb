// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/pnacl_translation_cache.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using base::FilePath;

namespace pnacl_cache {

class PNaClTranslationCacheTest : public testing::Test {
 protected:
  PNaClTranslationCacheTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}
  virtual ~PNaClTranslationCacheTest() {}
  virtual void SetUp() { cache_ = new PNaClTranslationCache(); }
  virtual void TearDown() {
    // The destructor of PNaClTranslationCacheWriteEntry posts a task to the IO
    // thread to close the backend cache entry. We want to make sure the entries
    // are closed before we delete the backend (and in particular the destructor
    // for the memory backend has a DCHECK to verify this), so we run the loop
    // here to ensure the task gets processed.
    base::RunLoop().RunUntilIdle();
    delete cache_;
  }

  void InitBackend(bool in_mem);
  void StoreNexe(const std::string& key, const std::string& nexe);
  std::string GetNexe(const std::string& key);

 protected:
  PNaClTranslationCache* cache_;
  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir temp_dir_;
};

void PNaClTranslationCacheTest::InitBackend(bool in_mem) {
  net::TestCompletionCallback init_cb;
  if (!in_mem) {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }
  int rv = cache_->InitCache(temp_dir_.path(), in_mem, init_cb.callback());
  if (in_mem)
    ASSERT_EQ(net::OK, rv);
  ASSERT_EQ(net::OK, init_cb.GetResult(rv));
  ASSERT_EQ(0, cache_->Size());
}

void PNaClTranslationCacheTest::StoreNexe(const std::string& key,
                                          const std::string& nexe) {
  net::TestCompletionCallback store_cb;
  cache_->StoreNexe(key, nexe, store_cb.callback());
  // Using ERR_IO_PENDING here causes the callback to wait for the result
  // which should be harmless even if it returns OK immediately. This is because
  // we don't plumb the intermediate writing stages all the way out.
  EXPECT_EQ(net::OK, store_cb.GetResult(net::ERR_IO_PENDING));
}

std::string PNaClTranslationCacheTest::GetNexe(const std::string& key) {
  net::TestCompletionCallback load_cb;
  std::string nexe;
  cache_->GetNexe(key, &nexe, load_cb.callback());
  EXPECT_EQ(net::OK, load_cb.GetResult(net::ERR_IO_PENDING));
  return nexe;
}

static const std::string test_key("1");
static const std::string test_store_val("testnexe");
static const int kLargeNexeSize = 16 * 1024 *1024;

TEST_F(PNaClTranslationCacheTest, StoreSmallInMem) {
  // Test that a single store puts something in the mem backend
  InitBackend(true);
  StoreNexe(test_key, test_store_val);
  EXPECT_EQ(1, cache_->Size());
}

TEST_F(PNaClTranslationCacheTest, StoreSmallOnDisk) {
  // Test that a single store puts something in the disk backend
  InitBackend(false);
  StoreNexe(test_key, test_store_val);
  EXPECT_EQ(1, cache_->Size());
}

TEST_F(PNaClTranslationCacheTest, StoreLargeOnDisk) {
  // Test a value too large(?) for a single I/O operation
  // TODO(dschuff): we only seem to ever have one operation go through into the
  // backend. Find out what the 'offset' field means, and if it can ever require
  // multiple writes.
  InitBackend(false);
  const std::string large_buffer(kLargeNexeSize, 'a');
  StoreNexe(test_key, large_buffer);
  EXPECT_EQ(1, cache_->Size());
}

TEST_F(PNaClTranslationCacheTest, InMemSizeLimit) {
  InitBackend(true);
  const std::string large_buffer(kMaxMemCacheSize + 1, 'a');
  net::TestCompletionCallback store_cb;
  cache_->StoreNexe(test_key, large_buffer, store_cb.callback());
  EXPECT_EQ(net::ERR_FAILED, store_cb.GetResult(net::ERR_IO_PENDING));
  base::RunLoop().RunUntilIdle();  // Ensure the entry is closed.
  EXPECT_EQ(0, cache_->Size());
}

TEST_F(PNaClTranslationCacheTest, GetOneInMem) {
  InitBackend(true);
  StoreNexe(test_key, test_store_val);
  EXPECT_EQ(1, cache_->Size());
  EXPECT_EQ(0, GetNexe(test_key).compare(test_store_val));
}

TEST_F(PNaClTranslationCacheTest, GetLargeOnDisk) {
  InitBackend(false);
  const std::string large_buffer(kLargeNexeSize, 'a');
  StoreNexe(test_key, large_buffer);
  EXPECT_EQ(1, cache_->Size());
  EXPECT_EQ(0, GetNexe(test_key).compare(large_buffer));
}

TEST_F(PNaClTranslationCacheTest, StoreTwice) {
  // Test that storing twice with the same key overwrites
  InitBackend(true);
  StoreNexe(test_key, test_store_val);
  StoreNexe(test_key, test_store_val + "aaa");
  EXPECT_EQ(1, cache_->Size());
  EXPECT_EQ(0, GetNexe(test_key).compare(test_store_val + "aaa"));
}

TEST_F(PNaClTranslationCacheTest, StoreTwo) {
  InitBackend(true);
  StoreNexe(test_key, test_store_val);
  StoreNexe(test_key + "a", test_store_val + "aaa");
  EXPECT_EQ(2, cache_->Size());
  EXPECT_EQ(0, GetNexe(test_key).compare(test_store_val));
  EXPECT_EQ(0, GetNexe(test_key + "a").compare(test_store_val + "aaa"));
}

TEST_F(PNaClTranslationCacheTest, GetMiss) {
  InitBackend(true);
  StoreNexe(test_key, test_store_val);
  net::TestCompletionCallback load_cb;
  std::string nexe;
  cache_->GetNexe(test_key + "a", &nexe, load_cb.callback());
  EXPECT_EQ(net::ERR_FAILED, load_cb.GetResult(net::ERR_IO_PENDING));
}

}  // namespace pnacl_cache
