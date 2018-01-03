// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/start_offline_page_upgrade_task.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/model/offline_page_item_generator.h"
#include "components/offline_pages/core/offline_page_metadata_store_test_util.h"
#include "components/offline_pages/core/test_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
const char kTestDigest[] = "TestDigest==";
}  // namespace

class StartOfflinePageUpgradeTaskTest : public testing::Test {
 public:
  StartOfflinePageUpgradeTaskTest();
  ~StartOfflinePageUpgradeTaskTest() override;

  void SetUp() override;
  void TearDown() override;

  void StartUpgradeDone(StartUpgradeResult result);

  OfflinePageMetadataStoreSQL* store() { return store_test_util_.store(); }
  OfflinePageMetadataStoreTestUtil* store_test_util() {
    return &store_test_util_;
  }
  OfflinePageItemGenerator* generator() { return &generator_; }
  TestTaskRunner* runner() { return &runner_; }

  StartUpgradeCallback callback() {
    return base::BindOnce(&StartOfflinePageUpgradeTaskTest::StartUpgradeDone,
                          base::Unretained(this));
  }

  StartUpgradeResult* last_result() { return &last_result_; }

 private:
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  OfflinePageMetadataStoreTestUtil store_test_util_;
  OfflinePageItemGenerator generator_;
  TestTaskRunner runner_;
  StartUpgradeResult last_result_;
};

StartOfflinePageUpgradeTaskTest::StartOfflinePageUpgradeTaskTest()
    : task_runner_(new base::TestMockTimeTaskRunner()),
      task_runner_handle_(task_runner_),
      store_test_util_(task_runner_),
      runner_(task_runner_),
      last_result_(StartUpgradeStatus::DB_ERROR) {}

StartOfflinePageUpgradeTaskTest::~StartOfflinePageUpgradeTaskTest() {}

void StartOfflinePageUpgradeTaskTest::SetUp() {
  store_test_util_.BuildStoreInMemory();
}

void StartOfflinePageUpgradeTaskTest::TearDown() {
  store_test_util_.DeleteStore();
}

void StartOfflinePageUpgradeTaskTest::StartUpgradeDone(
    StartUpgradeResult result) {
  last_result_ = std::move(result);
}

TEST_F(StartOfflinePageUpgradeTaskTest, StartUpgradeSuccess) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  generator()->SetArchiveDirectory(temp_dir.GetPath());

  OfflinePageItem original_page = generator()->CreateItemWithTempFile();
  original_page.upgrade_attempt = 3;
  original_page.digest = kTestDigest;
  store_test_util()->InsertItem(original_page);

  auto task = std::make_unique<StartOfflinePageUpgradeTask>(
      store(), original_page.offline_id, temp_dir.GetPath(), callback());
  runner()->RunTask(std::move(task));

  EXPECT_EQ(StartUpgradeStatus::SUCCESS, last_result()->status);
  EXPECT_EQ(kTestDigest, last_result()->digest);
  EXPECT_EQ(original_page.file_path, last_result()->file_path);

  auto upgraded_page =
      store_test_util()->GetPageByOfflineId(original_page.offline_id);
  ASSERT_TRUE(upgraded_page);
  EXPECT_EQ(2, upgraded_page->upgrade_attempt);
}

TEST_F(StartOfflinePageUpgradeTaskTest, StartUpgradeItemMissing) {
  auto task = std::make_unique<StartOfflinePageUpgradeTask>(
      store(), 42, base::FilePath(), callback());
  runner()->RunTask(std::move(task));

  EXPECT_EQ(StartUpgradeStatus::ITEM_MISSING, last_result()->status);
  EXPECT_TRUE(last_result()->digest.empty());
  EXPECT_TRUE(last_result()->file_path.empty());
}

TEST_F(StartOfflinePageUpgradeTaskTest, StartUpgradeFileMissing) {
  OfflinePageItem original_page = generator()->CreateItem();
  original_page.upgrade_attempt = 3;
  store_test_util()->InsertItem(original_page);

  auto task = std::make_unique<StartOfflinePageUpgradeTask>(
      store(), original_page.offline_id, base::FilePath(), callback());
  runner()->RunTask(std::move(task));

  EXPECT_EQ(StartUpgradeStatus::FILE_MISSING, last_result()->status);
  EXPECT_TRUE(last_result()->digest.empty());
  EXPECT_TRUE(last_result()->file_path.empty());

  auto upgraded_page =
      store_test_util()->GetPageByOfflineId(original_page.offline_id);
  ASSERT_TRUE(upgraded_page);
  EXPECT_EQ(original_page, *upgraded_page);
}

TEST_F(StartOfflinePageUpgradeTaskTest, StartUpgradeNotEnoughSpace) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  generator()->SetArchiveDirectory(temp_dir.GetPath());

  OfflinePageItem original_page = generator()->CreateItemWithTempFile();
  original_page.upgrade_attempt = 3;
  store_test_util()->InsertItem(original_page);

  auto task = std::make_unique<StartOfflinePageUpgradeTask>(
      store(), original_page.offline_id, base::FilePath(), callback());
  runner()->RunTask(std::move(task));

  EXPECT_EQ(StartUpgradeStatus::NOT_ENOUGH_STORAGE, last_result()->status);
  EXPECT_TRUE(last_result()->digest.empty());
  EXPECT_TRUE(last_result()->file_path.empty());

  auto upgraded_page =
      store_test_util()->GetPageByOfflineId(original_page.offline_id);
  ASSERT_TRUE(upgraded_page);
  EXPECT_EQ(original_page, *upgraded_page);
}

}  // namespace offline_pages
