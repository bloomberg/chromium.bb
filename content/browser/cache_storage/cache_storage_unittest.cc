// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/thread_task_runner_handle.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
const char kOrigin[] = "http://example.com/";
}

class TestCacheStorage : public CacheStorage {
 public:
  explicit TestCacheStorage(const base::FilePath& file_path)
      : CacheStorage(file_path,
                     false /* memory_only */,
                     base::ThreadTaskRunnerHandle::Get().get(),
                     scoped_refptr<net::URLRequestContextGetter>(),
                     scoped_refptr<storage::QuotaManagerProxy>(),
                     base::WeakPtr<storage::BlobStorageContext>(),
                     GURL(kOrigin)),
        delay_preserved_cache_callback_(false) {}

  void RunPreservedCacheCallback() {
    ASSERT_FALSE(preserved_cache_callback_.is_null());
    preserved_cache_callback_.Run();
    preserved_cache_callback_.Reset();
  }

  void set_delay_preserved_cache_callback(bool should_delay) {
    delay_preserved_cache_callback_ = should_delay;
  }

  bool IsPreservingCache(const CacheStorageCache* cache) {
    return ContainsKey(preserved_caches_, cache);
  }

 private:
  void SchedulePreservedCacheRemoval(const base::Closure& callback) override {
    if (!delay_preserved_cache_callback_) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
      return;
    }
    preserved_cache_callback_ = callback;
  }

  bool delay_preserved_cache_callback_;
  base::Closure preserved_cache_callback_;
};

class CacheStorageTest : public testing::Test {
 protected:
  CacheStorageTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    test_cache_storage_.reset(new TestCacheStorage(temp_dir_.path()));
  }

  bool OpenCache(const std::string& cache_name) {
    bool callback_called = false;
    test_cache_storage_->OpenCache(
        cache_name, base::Bind(&CacheStorageTest::OpenCacheCallback,
                               base::Unretained(this), &callback_called));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(callback_called);
    return callback_error_ == CACHE_STORAGE_OK;
  }

  void OpenCacheCallback(bool* callback_called,
                         const scoped_refptr<CacheStorageCache>& cache,
                         CacheStorageError error) {
    *callback_called = true;
    callback_cache_ = cache;
    callback_error_ = error;
  }

  base::ScopedTempDir temp_dir_;
  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<TestCacheStorage> test_cache_storage_;

  scoped_refptr<CacheStorageCache> callback_cache_;
  CacheStorageError callback_error_;
};

TEST_F(CacheStorageTest, PreserveCache) {
  test_cache_storage_->set_delay_preserved_cache_callback(true);
  EXPECT_TRUE(OpenCache("foo"));
  EXPECT_TRUE(test_cache_storage_->IsPreservingCache(callback_cache_.get()));
  test_cache_storage_->RunPreservedCacheCallback();
  EXPECT_FALSE(test_cache_storage_->IsPreservingCache(callback_cache_.get()));

  // Try opening it again, since the cache is already open (callback_cache_ is
  // referencing it) the cache shouldn't be preserved again.
  EXPECT_TRUE(OpenCache("foo"));
  EXPECT_FALSE(test_cache_storage_->IsPreservingCache(callback_cache_.get()));

  // Remove the reference to the cache so that it's deleted. After that, the
  // next time it's opened will require the cache to be created again and
  // preserved.
  callback_cache_ = nullptr;
  EXPECT_TRUE(OpenCache("foo"));
  EXPECT_TRUE(test_cache_storage_->IsPreservingCache(callback_cache_.get()));
  test_cache_storage_->RunPreservedCacheCallback();
  EXPECT_FALSE(test_cache_storage_->IsPreservingCache(callback_cache_.get()));
}

}  // namespace content
