// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/add_unique_urls_task.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {
const char kTestNamespace[] = "test";
const char kClientId1[] = "ID-1";
const char kClientId2[] = "ID-2";
const char kClientId3[] = "ID-3";
const char kClientId4[] = "ID-5";
const GURL kTestURL1("https://www.google.com/");
const GURL kTestURL2("http://www.example.com/");
const GURL kTestURL3("https://news.google.com/");
const GURL kTestURL4("https://chrome.google.com/");
const base::string16 kTestTitle1 = base::ASCIIToUTF16("Title 1");
const base::string16 kTestTitle2 = base::ASCIIToUTF16("Title 2");
const base::string16 kTestTitle3 = base::ASCIIToUTF16("Title 3");
const base::string16 kTestTitle4 = base::ASCIIToUTF16("Title 4");
}  // namespace

class AddUniqueUrlsTaskTest : public testing::Test {
 public:
  AddUniqueUrlsTaskTest();
  ~AddUniqueUrlsTaskTest() override = default;

  void SetUp() override;
  void TearDown() override;

  PrefetchStore* store() { return store_test_util_.store(); }

  PrefetchStoreTestUtil* store_util() { return &store_test_util_; }

  void PumpLoop();

  // Returns all items stored in a map keyed with client id.
  std::map<std::string, PrefetchItem> GetAllItems();

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

std::map<std::string, PrefetchItem> AddUniqueUrlsTaskTest::GetAllItems() {
  std::set<PrefetchItem> set;
  store_util()->GetAllItems(&set);

  std::map<std::string, PrefetchItem> map;
  for (const auto& item : set)
    map[item.client_id.id] = item;
  return map;
}

TEST_F(AddUniqueUrlsTaskTest, AddTaskInEmptyStore) {
  std::vector<PrefetchURL> urls;
  urls.push_back(PrefetchURL{kClientId1, kTestURL1, kTestTitle1});
  urls.push_back(PrefetchURL{kClientId2, kTestURL2, kTestTitle2});
  AddUniqueUrlsTask task(store(), kTestNamespace, urls);
  task.Run();
  PumpLoop();

  std::map<std::string, PrefetchItem> items = GetAllItems();
  ASSERT_EQ(2u, items.size());
  ASSERT_TRUE(items.count(kClientId1) > 0);
  EXPECT_EQ(kTestURL1, items[kClientId1].url);
  EXPECT_EQ(kTestNamespace, items[kClientId1].client_id.name_space);
  EXPECT_EQ(kTestTitle1, items[kClientId1].title);
  ASSERT_TRUE(items.count(kClientId2) > 0);
  EXPECT_EQ(kTestURL2, items[kClientId2].url);
  EXPECT_EQ(kTestNamespace, items[kClientId2].client_id.name_space);
  EXPECT_EQ(kTestTitle2, items[kClientId2].title);
}

TEST_F(AddUniqueUrlsTaskTest, DontAddURLIfItExists) {
  std::vector<PrefetchURL> urls;
  urls.push_back(PrefetchURL{kClientId1, kTestURL1, kTestTitle1});
  urls.push_back(PrefetchURL{kClientId2, kTestURL2, kTestTitle2});
  AddUniqueUrlsTask task1(store(), kTestNamespace, urls);
  task1.Run();
  PumpLoop();

  urls.clear();
  urls.push_back(PrefetchURL{kClientId4, kTestURL1, kTestTitle4});
  urls.push_back(PrefetchURL{kClientId3, kTestURL3, kTestTitle3});
  AddUniqueUrlsTask task2(store(), kTestNamespace, urls);
  task2.Run();
  PumpLoop();

  std::map<std::string, PrefetchItem> items = GetAllItems();
  ASSERT_EQ(3u, items.size());
  ASSERT_TRUE(items.count(kClientId1) > 0);
  EXPECT_EQ(kTestURL1, items[kClientId1].url);
  EXPECT_EQ(kTestNamespace, items[kClientId1].client_id.name_space);
  EXPECT_EQ(kTestTitle1, items[kClientId1].title);
  ASSERT_TRUE(items.count(kClientId2) > 0);
  EXPECT_EQ(kTestURL2, items[kClientId2].url);
  EXPECT_EQ(kTestNamespace, items[kClientId2].client_id.name_space);
  EXPECT_EQ(kTestTitle2, items[kClientId2].title);
  ASSERT_TRUE(items.count(kClientId3) > 0);
  EXPECT_EQ(kTestURL3, items[kClientId3].url);
  EXPECT_EQ(kTestNamespace, items[kClientId3].client_id.name_space);
  EXPECT_EQ(kTestTitle3, items[kClientId3].title);
}

TEST_F(AddUniqueUrlsTaskTest, HandleZombiePrefetchItems) {
  std::vector<PrefetchURL> urls;
  urls.push_back(PrefetchURL{kClientId1, kTestURL1, kTestTitle1});
  urls.push_back(PrefetchURL{kClientId2, kTestURL2, kTestTitle2});
  urls.push_back(PrefetchURL{kClientId3, kTestURL3, kTestTitle3});
  AddUniqueUrlsTask task1(store(), kTestNamespace, urls);
  task1.Run();
  PumpLoop();

  // ZombifyPrefetchItem returns the number of affected items.
  EXPECT_EQ(1, store_util()->ZombifyPrefetchItems(kTestNamespace, urls[0].url));
  EXPECT_EQ(1, store_util()->ZombifyPrefetchItems(kTestNamespace, urls[1].url));

  urls.clear();
  urls.push_back(PrefetchURL{kClientId1, kTestURL1, kTestTitle1});
  urls.push_back(PrefetchURL{kClientId3, kTestURL3, kTestTitle3});
  urls.push_back(PrefetchURL{kClientId4, kTestURL4, kTestTitle4});
  // ID-1 is expected to stay in zombie state.
  // ID-2 is expected to be removed, because it is in zombie state.
  // ID-3 is still requested, so it is ignored.
  // ID-4 is added.
  AddUniqueUrlsTask task2(store(), kTestNamespace, urls);
  task2.Run();
  PumpLoop();

  std::map<std::string, PrefetchItem> items = GetAllItems();
  ASSERT_EQ(3u, items.size());
  ASSERT_TRUE(items.count(kClientId1) > 0);
  EXPECT_EQ(kTestURL1, items[kClientId1].url);
  EXPECT_EQ(kTestNamespace, items[kClientId1].client_id.name_space);
  EXPECT_EQ(kTestTitle1, items[kClientId1].title);
  ASSERT_TRUE(items.count(kClientId3) > 0);
  EXPECT_EQ(kTestURL3, items[kClientId3].url);
  EXPECT_EQ(kTestNamespace, items[kClientId3].client_id.name_space);
  EXPECT_EQ(kTestTitle3, items[kClientId3].title);
  ASSERT_TRUE(items.count(kClientId4) > 0);
  EXPECT_EQ(kTestURL4, items[kClientId4].url);
  EXPECT_EQ(kTestNamespace, items[kClientId4].client_id.name_space);
  EXPECT_EQ(kTestTitle4, items[kClientId4].title);
}

}  // namespace offline_pages
