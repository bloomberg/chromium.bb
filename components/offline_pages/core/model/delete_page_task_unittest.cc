// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/delete_page_task.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/model/offline_page_item_generator.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_metadata_store_test_util.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/offline_store_types.h"
#include "components/offline_pages/core/test_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {

using DeletedPageInfo = OfflinePageModel::DeletedPageInfo;

namespace {

const char kTestNamespace[] = "default";
const GURL kTestUrl1("http://example.com");
const GURL kTestUrl2("http://other.page.com");
const int64_t kTestOfflineIdNoMatch = 20170905LL;
const ClientId kTestClientIdNoMatch(kTestNamespace, "20170905");

}  // namespace

class DeletePageTaskTest : public testing::Test,
                           public base::SupportsWeakPtr<DeletePageTaskTest> {
 public:
  DeletePageTaskTest();
  ~DeletePageTaskTest() override;

  void SetUp() override;
  void TearDown() override;

  void PumpLoop();
  void ResetResults();

  void OnDeletePageDone(DeletePageResult result,
                        const std::vector<DeletedPageInfo>& deleted_page_infos);
  bool CheckPageDeleted(const OfflinePageItem& page);
  DeletePageTask::DeletePageTaskCallback delete_page_callback();

  OfflinePageMetadataStoreTestUtil* store_test_util() {
    return &store_test_util_;
  }
  OfflinePageMetadataStoreSQL* store() { return store_test_util_.store(); }
  ClientPolicyController* policy_controller() {
    return policy_controller_.get();
  }
  OfflinePageItemGenerator* generator() { return &generator_; }
  TestTaskRunner* runner() { return &runner_; }
  DeletePageResult last_delete_page_result() {
    return last_delete_page_result_;
  }
  const std::vector<DeletedPageInfo>& last_deleted_page_infos() {
    return last_deleted_page_infos_;
  }
  const base::FilePath& temp_dir() { return temp_dir_.GetPath(); }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  OfflinePageMetadataStoreTestUtil store_test_util_;
  std::unique_ptr<ClientPolicyController> policy_controller_;
  OfflinePageItemGenerator generator_;
  TestTaskRunner runner_;
  base::ScopedTempDir temp_dir_;

  DeletePageResult last_delete_page_result_;
  std::vector<DeletedPageInfo> last_deleted_page_infos_;
};

DeletePageTaskTest::DeletePageTaskTest()
    : task_runner_(new base::TestSimpleTaskRunner()),
      task_runner_handle_(task_runner_),
      store_test_util_(task_runner_),
      runner_(task_runner_),
      last_delete_page_result_(DeletePageResult::RESULT_COUNT) {}

DeletePageTaskTest::~DeletePageTaskTest() {}

void DeletePageTaskTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  store_test_util_.BuildStoreInMemory();
  policy_controller_ = base::MakeUnique<ClientPolicyController>();
  generator()->SetArchiveDirectory(temp_dir());
}

void DeletePageTaskTest::TearDown() {
  store_test_util_.DeleteStore();
  if (temp_dir_.IsValid()) {
    if (!temp_dir_.Delete())
      DLOG(ERROR) << "temp_dir_ not created";
  }
  PumpLoop();
}

void DeletePageTaskTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

void DeletePageTaskTest::OnDeletePageDone(
    DeletePageResult result,
    const std::vector<DeletedPageInfo>& deleted_page_infos) {
  last_delete_page_result_ = result;
  last_deleted_page_infos_ = deleted_page_infos;
}

DeletePageTask::DeletePageTaskCallback
DeletePageTaskTest::delete_page_callback() {
  return base::BindOnce(&DeletePageTaskTest::OnDeletePageDone, AsWeakPtr());
}

bool DeletePageTaskTest::CheckPageDeleted(const OfflinePageItem& page) {
  auto stored_page = store_test_util()->GetPageByOfflineId(page.offline_id);
  return !base::PathExists(page.file_path) && !stored_page;
}

TEST_F(DeletePageTaskTest, DeletePageByOfflineId) {
  // Add 3 pages and try to delete 2 of them using offline id.
  generator()->SetNamespace(kTestNamespace);
  OfflinePageItem page1 = generator()->CreateItemWithTempFile();
  OfflinePageItem page2 = generator()->CreateItemWithTempFile();
  OfflinePageItem page3 = generator()->CreateItemWithTempFile();

  store_test_util()->InsertItem(page1);
  store_test_util()->InsertItem(page2);
  store_test_util()->InsertItem(page3);

  EXPECT_EQ(3LL, store_test_util()->GetPageCount());
  EXPECT_TRUE(base::PathExists(page1.file_path));
  EXPECT_TRUE(base::PathExists(page2.file_path));
  EXPECT_TRUE(base::PathExists(page3.file_path));

  // The pages with the offline ids will be removed from the store.
  std::vector<int64_t> offline_ids({page1.offline_id, page3.offline_id});
  auto task = DeletePageTask::CreateTaskMatchingOfflineIds(
      store(), offline_ids, delete_page_callback());
  runner()->RunTask(std::move(task));

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(2UL, last_deleted_page_infos().size());
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_TRUE(CheckPageDeleted(page1));
  EXPECT_FALSE(CheckPageDeleted(page2));
  EXPECT_TRUE(CheckPageDeleted(page3));
}

TEST_F(DeletePageTaskTest, DeletePageByOfflineIdNotFound) {
  generator()->SetNamespace(kTestNamespace);
  OfflinePageItem page1 = generator()->CreateItemWithTempFile();
  OfflinePageItem page2 = generator()->CreateItemWithTempFile();
  OfflinePageItem page3 = generator()->CreateItemWithTempFile();
  store_test_util()->InsertItem(page1);
  store_test_util()->InsertItem(page2);
  store_test_util()->InsertItem(page3);

  EXPECT_EQ(3LL, store_test_util()->GetPageCount());

  // The pages with the offline ids will be removed from the store. But since
  // the id isn't in the store, there will be no pages deleted and the result
  // will be success since there's no NOT_FOUND anymore.
  // This *might* break if any of the generated offline ids above equals to the
  // constant value defined above.
  std::vector<int64_t> offline_ids({kTestOfflineIdNoMatch});
  auto task = DeletePageTask::CreateTaskMatchingOfflineIds(
      store(), offline_ids, delete_page_callback());
  runner()->RunTask(std::move(task));

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(0UL, last_deleted_page_infos().size());
  EXPECT_EQ(3LL, store_test_util()->GetPageCount());
  EXPECT_FALSE(CheckPageDeleted(page1));
  EXPECT_FALSE(CheckPageDeleted(page2));
  EXPECT_FALSE(CheckPageDeleted(page3));
}

TEST_F(DeletePageTaskTest, DeletePageByClientId) {
  // Add 3 pages and try to delete 2 of them using client id.
  generator()->SetNamespace(kTestNamespace);
  OfflinePageItem page1 = generator()->CreateItemWithTempFile();
  OfflinePageItem page2 = generator()->CreateItemWithTempFile();
  OfflinePageItem page3 = generator()->CreateItemWithTempFile();
  store_test_util()->InsertItem(page1);
  store_test_util()->InsertItem(page2);
  store_test_util()->InsertItem(page3);

  EXPECT_EQ(3LL, store_test_util()->GetPageCount());
  EXPECT_TRUE(base::PathExists(page1.file_path));
  EXPECT_TRUE(base::PathExists(page2.file_path));
  EXPECT_TRUE(base::PathExists(page3.file_path));

  std::vector<ClientId> client_ids({page1.client_id, page3.client_id});
  auto task = DeletePageTask::CreateTaskMatchingClientIds(
      store(), client_ids, delete_page_callback());
  runner()->RunTask(std::move(task));

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(2UL, last_deleted_page_infos().size());
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_TRUE(CheckPageDeleted(page1));
  EXPECT_FALSE(CheckPageDeleted(page2));
  EXPECT_TRUE(CheckPageDeleted(page3));
}

TEST_F(DeletePageTaskTest, DeletePageByClientIdNotFound) {
  // Add 3 pages and try to delete 2 of them using client id.
  generator()->SetNamespace(kTestNamespace);
  OfflinePageItem page1 = generator()->CreateItemWithTempFile();
  OfflinePageItem page2 = generator()->CreateItemWithTempFile();
  OfflinePageItem page3 = generator()->CreateItemWithTempFile();
  store_test_util()->InsertItem(page1);
  store_test_util()->InsertItem(page2);
  store_test_util()->InsertItem(page3);

  EXPECT_EQ(3LL, store_test_util()->GetPageCount());

  // The pages with the client ids will be removed from the store. But since
  // the id isn't in the store, there will be no pages deleted and the result
  // will be success since there's no NOT_FOUND anymore.
  std::vector<ClientId> client_ids({kTestClientIdNoMatch});
  auto task = DeletePageTask::CreateTaskMatchingClientIds(
      store(), client_ids, delete_page_callback());
  runner()->RunTask(std::move(task));

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(0UL, last_deleted_page_infos().size());
  EXPECT_EQ(3LL, store_test_util()->GetPageCount());
}

TEST_F(DeletePageTaskTest, DeletePageByUrlPredicate) {
  // Add 3 pages and try to delete 2 of them using url predicate.
  generator()->SetNamespace(kTestNamespace);
  generator()->SetUrl(kTestUrl1);
  OfflinePageItem page1 = generator()->CreateItemWithTempFile();
  OfflinePageItem page2 = generator()->CreateItemWithTempFile();
  generator()->SetUrl(kTestUrl2);
  OfflinePageItem page3 = generator()->CreateItemWithTempFile();

  store_test_util()->InsertItem(page1);
  store_test_util()->InsertItem(page2);
  store_test_util()->InsertItem(page3);

  EXPECT_EQ(3LL, store_test_util()->GetPageCount());
  EXPECT_TRUE(base::PathExists(page1.file_path));
  EXPECT_TRUE(base::PathExists(page2.file_path));
  EXPECT_TRUE(base::PathExists(page3.file_path));

  // Delete all pages with url contains example.com, which are with kTestUrl1.
  UrlPredicate predicate = base::Bind([](const GURL& url) -> bool {
    return url.spec().find("example.com") != std::string::npos;
  });

  auto task = DeletePageTask::CreateTaskMatchingUrlPredicateForCachedPages(
      store(), policy_controller(), predicate, delete_page_callback());
  runner()->RunTask(std::move(task));

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(2UL, last_deleted_page_infos().size());
  EXPECT_EQ(predicate.Run(page1.url), CheckPageDeleted(page1));
  EXPECT_EQ(predicate.Run(page2.url), CheckPageDeleted(page2));
  EXPECT_EQ(predicate.Run(page3.url), CheckPageDeleted(page3));
}

TEST_F(DeletePageTaskTest, DeletePageByUrlPredicateNotFound) {
  // Add 3 pages and try to delete 2 of them using url predicate.
  generator()->SetNamespace(kTestNamespace);
  generator()->SetUrl(kTestUrl1);
  OfflinePageItem page1 = generator()->CreateItemWithTempFile();
  OfflinePageItem page2 = generator()->CreateItemWithTempFile();
  generator()->SetUrl(kTestUrl2);
  OfflinePageItem page3 = generator()->CreateItemWithTempFile();

  store_test_util()->InsertItem(page1);
  store_test_util()->InsertItem(page2);
  store_test_util()->InsertItem(page3);

  EXPECT_EQ(3LL, store_test_util()->GetPageCount());
  EXPECT_TRUE(base::PathExists(page1.file_path));
  EXPECT_TRUE(base::PathExists(page2.file_path));
  EXPECT_TRUE(base::PathExists(page3.file_path));

  // Return false for all pages so that no pages will be deleted.
  UrlPredicate predicate =
      base::Bind([](const GURL& url) -> bool { return false; });

  auto task = DeletePageTask::CreateTaskMatchingUrlPredicateForCachedPages(
      store(), policy_controller(), predicate, delete_page_callback());
  runner()->RunTask(std::move(task));

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(0UL, last_deleted_page_infos().size());
  EXPECT_FALSE(CheckPageDeleted(page1));
  EXPECT_FALSE(CheckPageDeleted(page2));
  EXPECT_FALSE(CheckPageDeleted(page3));
}

}  // namespace offline_pages
