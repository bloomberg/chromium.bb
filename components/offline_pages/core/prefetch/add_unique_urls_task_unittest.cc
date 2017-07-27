// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/add_unique_urls_task.h"

#include <string>
#include <vector>

#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
class AddUniqueUrlsTaskTest : public testing::Test {
 public:
  AddUniqueUrlsTaskTest();
  ~AddUniqueUrlsTaskTest() override = default;

  void SetUp() override;
  void TearDown() override;

  PrefetchStore* store() { return store_test_util_.store(); }

  PrefetchStoreTestUtil* store_util() { return &store_test_util_; }

  void PumpLoop();

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  PrefetchStoreTestUtil store_test_util_;
};

AddUniqueUrlsTaskTest::AddUniqueUrlsTaskTest()
    : task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_),
      store_test_util_(task_runner_) {}

void AddUniqueUrlsTaskTest::SetUp() {
  store_test_util_.BuildStoreInMemory();
}

void AddUniqueUrlsTaskTest::TearDown() {
  store_test_util_.DeleteStore();
  PumpLoop();
}

void AddUniqueUrlsTaskTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

TEST_F(AddUniqueUrlsTaskTest, AddTaskInEmptyStore) {
  std::string name_space("test");
  std::vector<PrefetchURL> urls;
  urls.push_back(PrefetchURL{"ID-1", GURL("https://www.google.com/")});
  urls.push_back(PrefetchURL{"ID-2", GURL("http://www.example.com/")});
  AddUniqueUrlsTask task(store(), name_space, urls);
  task.Run();
  PumpLoop();

  EXPECT_EQ(2, store_util()->CountPrefetchItems());
}

TEST_F(AddUniqueUrlsTaskTest, DontAddURLIfItExists) {
  std::string name_space("test");
  std::vector<PrefetchURL> urls;
  urls.push_back(PrefetchURL{"ID-1", GURL("https://www.google.com/")});
  urls.push_back(PrefetchURL{"ID-2", GURL("http://www.example.com/")});
  AddUniqueUrlsTask task1(store(), name_space, urls);
  task1.Run();
  PumpLoop();

  urls.clear();
  urls.push_back(PrefetchURL{"ID-1", GURL("https://www.google.com/")});
  urls.push_back(PrefetchURL{"ID-3", GURL("https://news.google.com/")});
  AddUniqueUrlsTask task2(store(), name_space, urls);
  task2.Run();
  PumpLoop();

  // Do the count here.
  EXPECT_EQ(3, store_util()->CountPrefetchItems());
}

TEST_F(AddUniqueUrlsTaskTest, HandleZombiePrefetchItems) {
  std::string name_space("test");
  std::vector<PrefetchURL> urls;
  urls.push_back(PrefetchURL{"ID-1", GURL("https://www.google.com/")});
  urls.push_back(PrefetchURL{"ID-2", GURL("http://www.example.com/")});
  urls.push_back(PrefetchURL{"ID-3", GURL("https://news.google.com/")});
  AddUniqueUrlsTask task1(store(), name_space, urls);
  task1.Run();
  PumpLoop();

  // ZombifyPrefetchItem returns the number of affected items.
  EXPECT_EQ(1, store_util()->ZombifyPrefetchItems(name_space, urls[0].url));
  EXPECT_EQ(1, store_util()->ZombifyPrefetchItems(name_space, urls[1].url));

  urls.clear();
  urls.push_back(PrefetchURL{"ID-1", GURL("https://www.google.com/")});
  urls.push_back(PrefetchURL{"ID-3", GURL("https://news.google.com/")});
  urls.push_back(PrefetchURL{"ID-4", GURL("https://chrome.google.com/")});
  // ID-1 is expected to stay in zombie state.
  // ID-2 is expected to be removed, because it is in zombie state.
  // ID-3 is still requested, so it is ignored.
  // ID-4 is added.
  AddUniqueUrlsTask task2(store(), name_space, urls);
  task2.Run();
  PumpLoop();

  // Do the count here.
  EXPECT_EQ(3, store_util()->CountPrefetchItems());
}

}  // namespace offline_pages
