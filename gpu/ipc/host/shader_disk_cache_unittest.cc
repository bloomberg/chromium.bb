// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "gpu/ipc/host/shader_disk_cache.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace {

const int kDefaultClientId = 42;
const char kCacheKey[] = "key";
const char kCacheValue[] = "cached value";

}  // namespace

class ShaderDiskCacheTest : public testing::Test {
 public:
  ShaderDiskCacheTest()
      : cache_thread_("CacheThread") {
    base::Thread::Options options;
    options.message_loop_type = base::MessageLoop::TYPE_IO;
    CHECK(cache_thread_.StartWithOptions(options));
    factory_ =
        base::MakeUnique<ShaderCacheFactory>(cache_thread_.task_runner());
  }

  ~ShaderDiskCacheTest() override {}

  const base::FilePath& cache_path() { return temp_dir_.GetPath(); }

  void InitCache() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    factory_->SetCacheInfo(kDefaultClientId, cache_path());
  }

  ShaderCacheFactory* factory() { return factory_.get(); }

 private:
  void TearDown() override { factory_->RemoveCacheInfo(kDefaultClientId); }

  std::unique_ptr<ShaderCacheFactory> factory_;
  base::ScopedTempDir temp_dir_;
  base::Thread cache_thread_;
  base::MessageLoopForIO message_loop_;

  DISALLOW_COPY_AND_ASSIGN(ShaderDiskCacheTest);
};

TEST_F(ShaderDiskCacheTest, ClearsCache) {
  InitCache();

  scoped_refptr<ShaderDiskCache> cache = factory()->Get(kDefaultClientId);
  ASSERT_TRUE(cache.get() != NULL);

  net::TestCompletionCallback available_cb;
  int rv = cache->SetAvailableCallback(available_cb.callback());
  ASSERT_EQ(net::OK, available_cb.GetResult(rv));
  EXPECT_EQ(0, cache->Size());

  cache->Cache(kCacheKey, kCacheValue);

  net::TestCompletionCallback complete_cb;
  rv = cache->SetCacheCompleteCallback(complete_cb.callback());
  ASSERT_EQ(net::OK, complete_cb.GetResult(rv));
  EXPECT_EQ(1, cache->Size());

  base::Time time;
  net::TestCompletionCallback clear_cb;
  rv = cache->Clear(time, time, clear_cb.callback());
  ASSERT_EQ(net::OK, clear_cb.GetResult(rv));
  EXPECT_EQ(0, cache->Size());
};

// For https://crbug.com/663589.
TEST_F(ShaderDiskCacheTest, SafeToDeleteCacheMidEntryOpen) {
  InitCache();

  // Create a cache and wait for it to open.
  scoped_refptr<ShaderDiskCache> cache = factory()->Get(kDefaultClientId);
  ASSERT_TRUE(cache.get() != NULL);
  net::TestCompletionCallback available_cb;
  int rv = cache->SetAvailableCallback(available_cb.callback());
  ASSERT_EQ(net::OK, available_cb.GetResult(rv));
  EXPECT_EQ(0, cache->Size());

  // Start writing an entry to the cache but delete it before the backend has
  // finished opening the entry. There is a race here, so this usually (but
  // not always) crashes if there is a problem.
  cache->Cache(kCacheKey, kCacheValue);
  cache = nullptr;

  // Open a new cache (to pass time on the cache thread) and verify all is
  // well.
  cache = factory()->Get(kDefaultClientId);
  ASSERT_TRUE(cache.get() != NULL);
  net::TestCompletionCallback available_cb2;
  int rv2 = cache->SetAvailableCallback(available_cb2.callback());
  ASSERT_EQ(net::OK, available_cb2.GetResult(rv2));
};

}  // namespace gpu
