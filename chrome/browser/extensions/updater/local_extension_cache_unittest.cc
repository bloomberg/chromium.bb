// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/local_extension_cache.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestExtensionId1[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kTestExtensionId2[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
const char kTestExtensionId3[] = "cccccccccccccccccccccccccccccccc";

}  // namespace

namespace extensions {

class LocalExtensionCacheTest : public testing::Test {
 public:
  LocalExtensionCacheTest() {}
  virtual ~LocalExtensionCacheTest() {}

  scoped_refptr<base::SequencedTaskRunner> background_task_runner() {
    return background_task_runner_;
  }

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    pool_owner_.reset(
        new base::SequencedWorkerPoolOwner(3, "Background Pool"));
    background_task_runner_ = pool_owner_->pool()->GetSequencedTaskRunner(
        pool_owner_->pool()->GetNamedSequenceToken("background"));
  }

  virtual void TearDown() OVERRIDE {
    pool_owner_->pool()->Shutdown();
    base::RunLoop().RunUntilIdle();
  }

  base::FilePath CreateCacheDir(bool initialized) {
    EXPECT_TRUE(cache_dir_.CreateUniqueTempDir());
    if (initialized)
      CreateFlagFile(cache_dir_.path());
    return cache_dir_.path();
  }

  base::FilePath CreateTempDir() {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    return temp_dir_.path();
  }

  void CreateFlagFile(const base::FilePath& dir) {
    CreateFile(dir.Append(
        extensions::LocalExtensionCache::kCacheReadyFlagFileName),
        0, base::Time::Now());
  }

  void CreateExtensionFile(const base::FilePath& dir,
                           const std::string& id,
                           const std::string& version,
                           size_t size,
                           const base::Time& timestamp) {
    CreateFile(GetExtensionFileName(dir, id, version), size, timestamp);
  }

  void CreateFile(const base::FilePath& file,
                  size_t size,
                  const base::Time& timestamp) {
    std::string data(size, 0);
    EXPECT_EQ(base::WriteFile(file, data.data(), data.size()), int(size));
    EXPECT_TRUE(base::TouchFile(file, timestamp, timestamp));
  }

  base::FilePath GetExtensionFileName(const base::FilePath& dir,
                                      const std::string& id,
                                      const std::string& version) {
    return dir.Append(id + "-" + version + ".crx");
  }

  void WaitForCompletion() {
    // In the worst case you need to repeat this up to 3 times to make sure that
    // all pending tasks we sent from UI thread to task runner and back to UI.
    for (int i = 0; i < 3; i++) {
      // Wait for background task completion that sends replay to UI thread.
      pool_owner_->pool()->FlushForTesting();
      // Wait for UI thread task completion.
      base::RunLoop().RunUntilIdle();
    }
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;

  scoped_ptr<base::SequencedWorkerPoolOwner> pool_owner_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  base::ScopedTempDir cache_dir_;
  base::ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(LocalExtensionCacheTest);
};

static void SimpleCallback(bool* ptr) {
  *ptr = true;
}

TEST_F(LocalExtensionCacheTest, Basic) {
  base::FilePath cache_dir(CreateCacheDir(false));

  LocalExtensionCache cache(cache_dir,
                            1000,
                            base::TimeDelta::FromDays(30),
                            background_task_runner());
  cache.SetCacheStatusPollingDelayForTests(base::TimeDelta());

  bool initialized = false;
  cache.Init(true, base::Bind(&SimpleCallback, &initialized));

  WaitForCompletion();
  EXPECT_FALSE(initialized);

  CreateExtensionFile(cache_dir, kTestExtensionId1, "1.0", 100,
                      base::Time::Now() - base::TimeDelta::FromDays(1));
  CreateExtensionFile(cache_dir, kTestExtensionId1, "0.1", 100,
                      base::Time::Now() - base::TimeDelta::FromDays(10));
  CreateExtensionFile(cache_dir, kTestExtensionId2, "2.0", 100,
                      base::Time::Now() - base::TimeDelta::FromDays(40));
  CreateExtensionFile(cache_dir, kTestExtensionId3, "3.0", 900,
                      base::Time::Now() - base::TimeDelta::FromDays(41));

  CreateFlagFile(cache_dir);

  WaitForCompletion();
  ASSERT_TRUE(initialized);

  // Older version should be removed on cache initialization.
  EXPECT_FALSE(base::PathExists(
      GetExtensionFileName(cache_dir, kTestExtensionId1, "0.1")));

  // All extensions should be there because cleanup happens on shutdown to
  // support use case when device was not used to more than 30 days and cache
  // shouldn't be cleaned before someone will have a chance to use it.
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId1, NULL, NULL));
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId2, NULL, NULL));
  EXPECT_TRUE(cache.GetExtension(kTestExtensionId3, NULL, NULL));

  bool did_shutdown = false;
  cache.Shutdown(base::Bind(&SimpleCallback, &did_shutdown));
  WaitForCompletion();
  ASSERT_TRUE(did_shutdown);

  EXPECT_TRUE(base::PathExists(
      GetExtensionFileName(cache_dir, kTestExtensionId1, "1.0")));
  EXPECT_FALSE(base::PathExists(
      GetExtensionFileName(cache_dir, kTestExtensionId2, "2.0")));
  EXPECT_FALSE(base::PathExists(
      GetExtensionFileName(cache_dir, kTestExtensionId3, "3.0")));
}

}  // namespace extensions
