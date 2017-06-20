// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/printing/ppd_cache.h"
#include "net/url_request/test_url_request_interceptor.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace printing {
namespace {

// This fixture just points the cache at a temporary directory for the life of
// the test.
class PpdCacheTest : public ::testing::Test {
 public:
  PpdCacheTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    ASSERT_TRUE(ppd_cache_temp_dir_.CreateUniqueTempDir());
  }

  // Make and return a cache for the test that uses a temporary directory
  // which is cleaned up at the end of the test.  Note that we pass
  // a (nonexistant) subdirectory of temp_dir_ to the cache to exercise
  // the lazy-creation-of-the-cache-directory code.
  scoped_refptr<PpdCache> CreateTestCache() {
    return PpdCache::Create(ppd_cache_temp_dir_.GetPath().Append("Cache"));
  }

  void CaptureFindResult(const PpdCache::FindResult& result) {
    ++captured_find_results_;
    find_result_ = result;
  }

  void CaptureStoreResult() { ++captured_store_results_; }

 protected:
  // Environment for task schedulers.
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  // Number of find results we've captured.
  int captured_find_results_ = 0;

  // Most recent captured result.
  PpdCache::FindResult find_result_;

  // Number of store callbacks we've seen.
  int captured_store_results_ = 0;

  // Overrider for DIR_CHROMEOS_PPD_CACHE that points it at a temporary
  // directory for the life of the test.
  base::ScopedTempDir ppd_cache_temp_dir_;
};


// Test that we miss on an empty cache.
TEST_F(PpdCacheTest, SimpleMiss) {
  auto cache = CreateTestCache();
  cache->Find("foo", base::Bind(&PpdCacheTest::CaptureFindResult,
                                base::Unretained(this)));
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(captured_find_results_, 1);
  EXPECT_FALSE(find_result_.success);
}

TEST_F(PpdCacheTest, MissThenHit) {
  auto cache = CreateTestCache();
  const char kTestKey[] = "My totally awesome key";
  const char kTestKey2[] = "A different key";
  const char kTestContents[] = "Like, totally awesome contents";

  cache->Find(kTestKey, base::Bind(&PpdCacheTest::CaptureFindResult,
                                   base::Unretained(this)));
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(captured_find_results_, 1);
  EXPECT_FALSE(find_result_.success);

  cache->Store(
      kTestKey, kTestContents,
      base::Bind(&PpdCacheTest::CaptureStoreResult, base::Unretained(this)));
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(captured_store_results_, 1);

  cache->Find(kTestKey, base::Bind(&PpdCacheTest::CaptureFindResult,
                                   base::Unretained(this)));
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(captured_find_results_, 2);
  EXPECT_TRUE(find_result_.success);
  EXPECT_EQ(find_result_.contents, kTestContents);
  EXPECT_LT(find_result_.age, base::TimeDelta::FromMinutes(5));

  cache->Find(kTestKey2, base::Bind(&PpdCacheTest::CaptureFindResult,
                                    base::Unretained(this)));
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(captured_find_results_, 3);
  EXPECT_FALSE(find_result_.success);
}

}  // namespace
}  // namespace printing
}  // namespace chromeos
