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
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_metadata_store_sql.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/offline_store_types.h"
#include "components/offline_pages/core/test_task.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#include "components/offline_pages/core/model/add_page_task.h"

namespace offline_pages {

using DeletedPageInfo = OfflinePageModel::DeletedPageInfo;

namespace {
const char kTestClientNamespace[] = "default";
const GURL kTestUrl1("http://example.com");
const GURL kTestUrl2("http://other.page.com");
const int64_t kTestOfflineId1 = 1234LL;
const int64_t kTestOfflineId2 = 890714LL;
const int64_t kTestOfflineId3 = 41LL;
const int64_t kTestOfflineIdNoMatch = 20170905LL;
const ClientId kTestClientId1(kTestClientNamespace, "1234");
const ClientId kTestClientId2(kTestClientNamespace, "890714");
const ClientId kTestClientId3(kTestClientNamespace, "41");
const ClientId kTestClientIdNoMatch(kTestClientNamespace, "20170905");
const base::FilePath kTestFilePath(FILE_PATH_LITERAL("/test/path/file"));
const int64_t kTestFileSize = 876543LL;

void OnGetAllPagesDone(MultipleOfflinePageItemResult* result,
                       MultipleOfflinePageItemResult pages) {
  (*result).swap(pages);
}

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
  void AddPage(OfflinePageItem& page);
  MultipleOfflinePageItemResult GetAllPages();

  OfflinePageMetadataStoreSQL* store() { return store_.get(); }
  DeletePageResult last_delete_page_result() {
    return last_delete_page_result_;
  }
  const std::vector<DeletedPageInfo>& last_deleted_page_infos() {
    return last_deleted_page_infos_;
  }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  std::unique_ptr<OfflinePageMetadataStoreSQL> store_;
  base::ScopedTempDir temp_dir_;

  DeletePageResult last_delete_page_result_;
  std::vector<DeletedPageInfo> last_deleted_page_infos_;
};

DeletePageTaskTest::DeletePageTaskTest()
    : task_runner_(new base::TestSimpleTaskRunner()),
      task_runner_handle_(task_runner_),
      last_delete_page_result_(DeletePageResult::RESULT_COUNT) {}

DeletePageTaskTest::~DeletePageTaskTest() {}

void DeletePageTaskTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  store_ = base::MakeUnique<OfflinePageMetadataStoreSQL>(
      base::ThreadTaskRunnerHandle::Get(), temp_dir_.GetPath());
  store_->Initialize(base::Bind([](bool result) {}));
  PumpLoop();
}

void DeletePageTaskTest::TearDown() {
  store_.reset();
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

MultipleOfflinePageItemResult DeletePageTaskTest::GetAllPages() {
  MultipleOfflinePageItemResult result;
  store_->GetOfflinePages(
      base::Bind(&OnGetAllPagesDone, base::Unretained(&result)));
  PumpLoop();
  return result;
}

void DeletePageTaskTest::AddPage(OfflinePageItem& page) {
  // Create a temporary file as the archived MHTML file. And update the page
  // with the newly generated file path.
  base::FilePath temp_file_path;
  ASSERT_TRUE(
      base::CreateTemporaryFileInDir(temp_dir_.GetPath(), &temp_file_path));
  page.file_path = temp_file_path;

  AddPageTask task(
      store(), page,
      base::Bind([](AddPageResult result, const OfflinePageItem& offline_page) {
        EXPECT_EQ(AddPageResult::SUCCESS, result);
      }));
  task.Run();
  PumpLoop();
}

TEST_F(DeletePageTaskTest, DeletePageByOfflineId) {
  // Add 3 pages and try to delete 2 of them using offline id.
  OfflinePageItem page(kTestUrl1, kTestOfflineId1, kTestClientId1,
                       kTestFilePath, kTestFileSize);
  OfflinePageItem page2(kTestUrl2, kTestOfflineId2, kTestClientId2,
                        kTestFilePath, kTestFileSize);
  OfflinePageItem page3(kTestUrl1, kTestOfflineId3, kTestClientId3,
                        kTestFilePath, kTestFileSize);
  AddPage(page);
  AddPage(page2);
  AddPage(page3);

  MultipleOfflinePageItemResult pages = GetAllPages();
  EXPECT_EQ(3UL, pages.size());
  EXPECT_TRUE(base::PathExists(page.file_path));
  EXPECT_TRUE(base::PathExists(page2.file_path));
  EXPECT_TRUE(base::PathExists(page3.file_path));

  // The pages with the offline ids will be removed from the store.
  std::vector<int64_t> offline_ids({kTestOfflineId1, kTestOfflineId3});
  std::unique_ptr<DeletePageTask> task(
      DeletePageTask::CreateTaskMatchingOfflineIds(
          store(), offline_ids,
          base::Bind(&DeletePageTaskTest::OnDeletePageDone, AsWeakPtr())));
  task->Run();
  PumpLoop();

  pages = GetAllPages();
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(1UL, pages.size());
  EXPECT_EQ(kTestOfflineId2, pages[0].offline_id);
  EXPECT_FALSE(base::PathExists(page.file_path));
  EXPECT_TRUE(base::PathExists(page2.file_path));
  EXPECT_FALSE(base::PathExists(page3.file_path));
}

TEST_F(DeletePageTaskTest, DeletePageByOfflineIdNotFound) {
  // Add 3 pages and try to delete 2 of them using offline id.
  OfflinePageItem page(kTestUrl1, kTestOfflineId1, kTestClientId1,
                       kTestFilePath, kTestFileSize);
  OfflinePageItem page2(kTestUrl2, kTestOfflineId2, kTestClientId2,
                        kTestFilePath, kTestFileSize);
  OfflinePageItem page3(kTestUrl1, kTestOfflineId3, kTestClientId3,
                        kTestFilePath, kTestFileSize);
  AddPage(page);
  AddPage(page2);
  AddPage(page3);

  MultipleOfflinePageItemResult pages = GetAllPages();
  EXPECT_EQ(3UL, pages.size());

  // The pages with the offline ids will be removed from the store. But since
  // the id isn't in the store, there will be no pages deleted and the result
  // will be success since there's no NOT_FOUND anymore.
  std::vector<int64_t> offline_ids({kTestOfflineIdNoMatch});
  std::unique_ptr<DeletePageTask> task(
      DeletePageTask::CreateTaskMatchingOfflineIds(
          store(), offline_ids,
          base::Bind(&DeletePageTaskTest::OnDeletePageDone, AsWeakPtr())));
  task->Run();
  PumpLoop();

  pages = GetAllPages();
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(3UL, pages.size());
}

TEST_F(DeletePageTaskTest, DeletePageByClientId) {
  // Add 3 pages and try to delete 2 of them using client id.
  OfflinePageItem page(kTestUrl1, kTestOfflineId1, kTestClientId1,
                       kTestFilePath, kTestFileSize);
  OfflinePageItem page2(kTestUrl2, kTestOfflineId2, kTestClientId2,
                        kTestFilePath, kTestFileSize);
  OfflinePageItem page3(kTestUrl1, kTestOfflineId3, kTestClientId3,
                        kTestFilePath, kTestFileSize);
  AddPage(page);
  AddPage(page2);
  AddPage(page3);

  MultipleOfflinePageItemResult pages = GetAllPages();
  EXPECT_EQ(3UL, pages.size());
  EXPECT_TRUE(base::PathExists(page.file_path));
  EXPECT_TRUE(base::PathExists(page2.file_path));
  EXPECT_TRUE(base::PathExists(page3.file_path));

  std::vector<ClientId> client_ids({kTestClientId1, kTestClientId3});
  std::unique_ptr<DeletePageTask> task(
      DeletePageTask::CreateTaskMatchingClientIds(
          store(), client_ids,
          base::Bind(&DeletePageTaskTest::OnDeletePageDone, AsWeakPtr())));
  task->Run();
  PumpLoop();

  pages = GetAllPages();
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(1UL, pages.size());
  EXPECT_EQ(kTestClientId2, pages[0].client_id);
  EXPECT_FALSE(base::PathExists(page.file_path));
  EXPECT_TRUE(base::PathExists(page2.file_path));
  EXPECT_FALSE(base::PathExists(page3.file_path));
}

TEST_F(DeletePageTaskTest, DeletePageByClientIdNotFound) {
  // Add 3 pages and try to delete 2 of them using offline id.
  OfflinePageItem page(kTestUrl1, kTestOfflineId1, kTestClientId1,
                       kTestFilePath, kTestFileSize);
  OfflinePageItem page2(kTestUrl2, kTestOfflineId2, kTestClientId2,
                        kTestFilePath, kTestFileSize);
  OfflinePageItem page3(kTestUrl1, kTestOfflineId3, kTestClientId3,
                        kTestFilePath, kTestFileSize);
  AddPage(page);
  AddPage(page2);
  AddPage(page3);

  MultipleOfflinePageItemResult pages = GetAllPages();
  EXPECT_EQ(3UL, pages.size());

  // The pages with the offline ids will be removed from the store. But since
  // the id isn't in the store, there will be no pages deleted and the result
  // will be success since there's no NOT_FOUND anymore.
  std::vector<ClientId> client_ids({kTestClientIdNoMatch});
  std::unique_ptr<DeletePageTask> task(
      DeletePageTask::CreateTaskMatchingClientIds(
          store(), client_ids,
          base::Bind(&DeletePageTaskTest::OnDeletePageDone, AsWeakPtr())));
  task->Run();
  PumpLoop();

  pages = GetAllPages();
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(3UL, pages.size());
}

TEST_F(DeletePageTaskTest, DeletePageByUrlPredicate) {
  // Add 3 pages and try to delete 2 of them using client id.
  OfflinePageItem page(kTestUrl1, kTestOfflineId1, kTestClientId1,
                       kTestFilePath, kTestFileSize);
  OfflinePageItem page2(kTestUrl2, kTestOfflineId2, kTestClientId2,
                        kTestFilePath, kTestFileSize);
  OfflinePageItem page3(kTestUrl1, kTestOfflineId3, kTestClientId3,
                        kTestFilePath, kTestFileSize);
  AddPage(page);
  AddPage(page2);
  AddPage(page3);

  MultipleOfflinePageItemResult pages = GetAllPages();
  EXPECT_EQ(3UL, pages.size());
  EXPECT_TRUE(base::PathExists(page.file_path));
  EXPECT_TRUE(base::PathExists(page2.file_path));
  EXPECT_TRUE(base::PathExists(page3.file_path));

  // Delete all pages with url contains example.com.
  UrlPredicate predicate = base::Bind([](const GURL& url) -> bool {
    return url.spec().find("example.com") != std::string::npos;
  });

  std::unique_ptr<DeletePageTask> task(
      DeletePageTask::CreateTaskMatchingUrlPredicateForCachedPages(
          store(), predicate,
          base::Bind(&DeletePageTaskTest::OnDeletePageDone, AsWeakPtr())));
  task->Run();
  PumpLoop();

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_EQ(predicate.Run(page.url), !base::PathExists(page.file_path));
  EXPECT_EQ(predicate.Run(page2.url), !base::PathExists(page2.file_path));
  EXPECT_EQ(predicate.Run(page3.url), !base::PathExists(page3.file_path));
}

TEST_F(DeletePageTaskTest, DeletePageByUrlPredicateNotFound) {
  // Add 3 pages and try to delete 2 of them using client id.
  OfflinePageItem page(kTestUrl1, kTestOfflineId1, kTestClientId1,
                       kTestFilePath, kTestFileSize);
  OfflinePageItem page2(kTestUrl2, kTestOfflineId2, kTestClientId2,
                        kTestFilePath, kTestFileSize);
  OfflinePageItem page3(kTestUrl1, kTestOfflineId3, kTestClientId3,
                        kTestFilePath, kTestFileSize);
  AddPage(page);
  AddPage(page2);
  AddPage(page3);

  MultipleOfflinePageItemResult pages = GetAllPages();
  EXPECT_EQ(3UL, pages.size());
  EXPECT_TRUE(base::PathExists(page.file_path));
  EXPECT_TRUE(base::PathExists(page2.file_path));
  EXPECT_TRUE(base::PathExists(page3.file_path));

  // Return false for all pages so that no pages will be deleted.
  UrlPredicate predicate =
      base::Bind([](const GURL& url) -> bool { return false; });

  std::unique_ptr<DeletePageTask> task(
      DeletePageTask::CreateTaskMatchingUrlPredicateForCachedPages(
          store(), predicate,
          base::Bind(&DeletePageTaskTest::OnDeletePageDone, AsWeakPtr())));
  task->Run();
  PumpLoop();

  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
  EXPECT_TRUE(last_deleted_page_infos().empty());
  EXPECT_TRUE(base::PathExists(page.file_path));
  EXPECT_TRUE(base::PathExists(page2.file_path));
  EXPECT_TRUE(base::PathExists(page3.file_path));
}

}  // namespace offline_pages
