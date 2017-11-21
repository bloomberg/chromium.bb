// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/clear_legacy_temporary_pages_task.h"

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/model/offline_page_item_generator.h"
#include "components/offline_pages/core/model/offline_page_test_util.h"
#include "components/offline_pages/core/offline_page_metadata_store_test_util.h"
#include "components/offline_pages/core/test_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class ClearLegacyTemporaryPagesTaskTest : public testing::Test {
 public:
  ClearLegacyTemporaryPagesTaskTest();
  ~ClearLegacyTemporaryPagesTaskTest() override;

  void SetUp() override;
  void TearDown() override;

  OfflinePageItem AddPage(const std::string& name_space,
                          const base::FilePath& archive_dir);

  OfflinePageMetadataStoreSQL* store() { return store_test_util_.store(); }
  OfflinePageMetadataStoreTestUtil* store_test_util() {
    return &store_test_util_;
  }
  OfflinePageItemGenerator* generator() { return &generator_; }
  TestTaskRunner* runner() { return &runner_; }
  ClientPolicyController* policy_controller() {
    return policy_controller_.get();
  }

  const base::FilePath& legacy_archives_dir() {
    return legacy_archives_dir_.GetPath();
  }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;

  OfflinePageMetadataStoreTestUtil store_test_util_;
  OfflinePageItemGenerator generator_;
  TestTaskRunner runner_;
  std::unique_ptr<ClientPolicyController> policy_controller_;

  base::ScopedTempDir legacy_archives_dir_;
};

ClearLegacyTemporaryPagesTaskTest::ClearLegacyTemporaryPagesTaskTest()
    : task_runner_(new base::TestSimpleTaskRunner()),
      task_runner_handle_(task_runner_),
      store_test_util_(task_runner_),
      runner_(task_runner_) {}

ClearLegacyTemporaryPagesTaskTest::~ClearLegacyTemporaryPagesTaskTest() {}

void ClearLegacyTemporaryPagesTaskTest::SetUp() {
  store_test_util_.BuildStoreInMemory();
  ASSERT_TRUE(legacy_archives_dir_.CreateUniqueTempDir());
  policy_controller_ = base::MakeUnique<ClientPolicyController>();
}

void ClearLegacyTemporaryPagesTaskTest::TearDown() {
  store_test_util_.DeleteStore();
  if (!legacy_archives_dir_.Delete())
    DVLOG(1) << "ScopedTempDir deletion failed.";
}

OfflinePageItem ClearLegacyTemporaryPagesTaskTest::AddPage(
    const std::string& name_space,
    const base::FilePath& archive_dir) {
  generator()->SetNamespace(name_space);
  generator()->SetArchiveDirectory(archive_dir);
  OfflinePageItem page = generator()->CreateItemWithTempFile();
  store_test_util()->InsertItem(page);
  return page;
}

// This test is affected by https://crbug.com/725685, which only affects windows
// platform.
#if defined(OS_WIN)
#define MAYBE_TestDeletePageInLegacyArchivesDir \
  DISABLED_TestDeletePageInLegacyArchivesDir
#else
#define MAYBE_TestDeletePageInLegacyArchivesDir \
  TestDeletePageInLegacyArchivesDir
#endif
TEST_F(ClearLegacyTemporaryPagesTaskTest,
       MAYBE_TestDeletePageInLegacyArchivesDir) {
  OfflinePageItem page1 = AddPage(kLastNNamespace, legacy_archives_dir());
  OfflinePageItem page2 = AddPage(kDownloadNamespace, legacy_archives_dir());

  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  EXPECT_EQ(2UL, test_util::GetFileCountInDirectory(legacy_archives_dir()));

  auto task = base::MakeUnique<ClearLegacyTemporaryPagesTask>(
      store(), policy_controller(), legacy_archives_dir());
  runner()->RunTask(std::move(task));

  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(1UL, test_util::GetFileCountInDirectory(legacy_archives_dir()));
  EXPECT_FALSE(store_test_util()->GetPageByOfflineId(page1.offline_id));
}

}  // namespace offline_pages
