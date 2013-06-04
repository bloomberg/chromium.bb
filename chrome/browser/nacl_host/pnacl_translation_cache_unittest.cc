// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nacl_host/pnacl_translation_cache.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace pnacl_cache {

class PNaClTranslationCacheTest : public testing::Test {
 protected:
  PNaClTranslationCacheTest()
      : cache_thread_(BrowserThread::CACHE, &message_loop_),
        io_thread_(BrowserThread::IO, &message_loop_) {}
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

 protected:
  PNaClTranslationCache* cache_;
  base::MessageLoopForIO message_loop_;
  content::TestBrowserThread cache_thread_;
  content::TestBrowserThread io_thread_;
};

TEST_F(PNaClTranslationCacheTest, StoreOneInMem) {
  net::TestCompletionCallback init_cb;
  int rv = cache_->InitCache(base::FilePath(), true, init_cb.callback());
  EXPECT_EQ(net::OK, rv);
  ASSERT_EQ(net::OK, init_cb.GetResult(rv));
  net::TestCompletionCallback store_cb;
  EXPECT_EQ(0, cache_->Size());
  cache_->StoreNexe("1", "a", store_cb.callback());
  // Using ERR_IO_PENDING here causes the callback to wait for the result
  // which should be harmless even if it returns OK immediately. This is because
  // we don't plumb the intermediate writing stages all the way out.
  EXPECT_EQ(net::OK, store_cb.GetResult(net::ERR_IO_PENDING));
  EXPECT_EQ(1, cache_->Size());
}

TEST_F(PNaClTranslationCacheTest, StoreOneOnDisk) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  net::TestCompletionCallback init_cb;
  int rv = cache_->InitCache(temp_dir.path(), false, init_cb.callback());
  EXPECT_TRUE(rv);
  ASSERT_EQ(net::OK, init_cb.GetResult(rv));
  EXPECT_EQ(0, cache_->Size());
  net::TestCompletionCallback store_cb;
  cache_->StoreNexe("1", "a", store_cb.callback());
  EXPECT_EQ(net::OK, store_cb.GetResult(net::ERR_IO_PENDING));
  EXPECT_EQ(1, cache_->Size());
}

TEST_F(PNaClTranslationCacheTest, InMemSizeLimit) {
  net::TestCompletionCallback init_cb;
  int rv = cache_->InitCache(base::FilePath(), true, init_cb.callback());
  EXPECT_EQ(rv, net::OK);
  ASSERT_EQ(init_cb.GetResult(rv), net::OK);
  EXPECT_EQ(cache_->Size(), 0);
  std::string large_buffer(kMaxMemCacheSize + 1, 'a');
  net::TestCompletionCallback store_cb;
  cache_->StoreNexe("1", large_buffer, store_cb.callback());
  EXPECT_EQ(net::ERR_FAILED, store_cb.GetResult(net::ERR_IO_PENDING));
  base::RunLoop().RunUntilIdle(); // Ensure the entry is closed.
  EXPECT_EQ(0, cache_->Size());
}

}  // namespace nacl_cache
