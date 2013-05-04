// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/gpu/shader_disk_cache.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class TestClosureCallback {
 public:
  TestClosureCallback()
      : callback_(base::Bind(
          &TestClosureCallback::StopWaiting, base::Unretained(this))) {
  }

  void WaitForResult() {
    wait_run_loop_.reset(new base::RunLoop());
    wait_run_loop_->Run();
  }

  const base::Closure& callback() { return callback_; }

 private:
  void StopWaiting() {
    wait_run_loop_->Quit();
  }

  base::Closure callback_;
  scoped_ptr<base::RunLoop> wait_run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestClosureCallback);
};

const int kDefaultClientId = 42;
const char kCacheKey[] = "key";
const char kCacheValue[] = "cached value";

}  // namespace

class StoragePartitionShaderClearTest : public testing::Test {
 public:
  StoragePartitionShaderClearTest() :
      ui_thread_(BrowserThread::UI, &message_loop_),
      cache_thread_(BrowserThread::CACHE, &message_loop_),
      io_thread_(BrowserThread::IO, &message_loop_) {
  }

  virtual ~StoragePartitionShaderClearTest() {}

  const base::FilePath& cache_path() { return temp_dir_.path(); }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ShaderCacheFactory::GetInstance()->SetCacheInfo(kDefaultClientId,
                                                    cache_path());

    cache_ = ShaderCacheFactory::GetInstance()->Get(kDefaultClientId);
    ASSERT_TRUE(cache_.get() != NULL);
  }

  void InitCache() {
    net::TestCompletionCallback available_cb;
    int rv = cache_->SetAvailableCallback(available_cb.callback());
    ASSERT_EQ(net::OK, available_cb.GetResult(rv));
    EXPECT_EQ(0, cache_->Size());

    cache_->Cache(kCacheKey, kCacheValue);

    net::TestCompletionCallback complete_cb;

    rv = cache_->SetCacheCompleteCallback(complete_cb.callback());
    ASSERT_EQ(net::OK, complete_cb.GetResult(rv));
  }

  size_t Size() { return cache_->Size(); }

  base::MessageLoop* message_loop() { return &message_loop_; }

 private:
  virtual void TearDown() OVERRIDE {
    cache_ = NULL;
    ShaderCacheFactory::GetInstance()->RemoveCacheInfo(kDefaultClientId);
  }

  base::ScopedTempDir temp_dir_;
  base::MessageLoopForIO message_loop_;
  TestBrowserThread ui_thread_;
  TestBrowserThread cache_thread_;
  TestBrowserThread io_thread_;

  scoped_refptr<ShaderDiskCache> cache_;
};

void ClearData(content::StoragePartitionImpl* sp,
               const base::Closure& cb) {
  base::Time time;
  sp->AsyncClearDataBetween(content::StoragePartition::kShaderStorage,
                           time, time, cb);
}

TEST_F(StoragePartitionShaderClearTest, ClearShaderCache) {
  InitCache();
  EXPECT_EQ(1u, Size());

  TestClosureCallback clear_cb;
  StoragePartitionImpl sp(cache_path(), NULL, NULL, NULL, NULL, NULL, NULL);
  message_loop()->PostTask(FROM_HERE,
                           base::Bind(&ClearData, &sp, clear_cb.callback()));
  clear_cb.WaitForResult();
  EXPECT_EQ(0u, Size());
}

}  // namespace content
