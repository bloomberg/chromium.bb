// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/add_page_task.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/model/offline_page_item_generator.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_metadata_store_test_util.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/offline_store_types.h"
#include "components/offline_pages/core/test_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {

namespace {

const char kTestNamespace[] = "default";
const GURL kTestUrl1("http://example.com");
const GURL kTestUrl2("http://other.page.com");
const int64_t kTestOfflineId1 = 1234LL;
const ClientId kTestClientId1(kTestNamespace, "1234");
const base::FilePath kTestFilePath(FILE_PATH_LITERAL("/test/path/file"));
const int64_t kTestFileSize = 876543LL;
const std::string kTestOrigin("abc.xyz");
const base::string16 kTestTitle = base::UTF8ToUTF16("a title");
const int64_t kTestDownloadId = 767574LL;
const std::string kTestDigest("TesTIngDigEst==");

}  // namespace

class AddPageTaskTest : public testing::Test,
                        public base::SupportsWeakPtr<AddPageTaskTest> {
 public:
  AddPageTaskTest();
  ~AddPageTaskTest() override;

  void SetUp() override;
  void TearDown() override;

  void ResetResults();
  void OnAddPageDone(AddPageResult result);
  AddPageTask::AddPageTaskCallback add_page_callback();

  void AddPage(const OfflinePageItem& page);
  bool CheckPageStored(const OfflinePageItem& page);

  OfflinePageMetadataStoreTestUtil* store_test_util() {
    return &store_test_util_;
  }
  OfflinePageMetadataStoreSQL* store() { return store_test_util_.store(); }
  OfflinePageItemGenerator* generator() { return &generator_; }
  TestTaskRunner* runner() { return &runner_; }

  AddPageResult last_add_page_result() { return last_add_page_result_; }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  OfflinePageMetadataStoreTestUtil store_test_util_;
  OfflinePageItemGenerator generator_;
  TestTaskRunner runner_;

  AddPageResult last_add_page_result_;
};

AddPageTaskTest::AddPageTaskTest()
    : task_runner_(new base::TestSimpleTaskRunner()),
      task_runner_handle_(task_runner_),
      store_test_util_(task_runner_),
      runner_(task_runner_),
      last_add_page_result_(AddPageResult::RESULT_COUNT) {}

AddPageTaskTest::~AddPageTaskTest() {}

void AddPageTaskTest::SetUp() {
  store_test_util_.BuildStoreInMemory();
}

void AddPageTaskTest::TearDown() {
  store_test_util_.DeleteStore();
}

void AddPageTaskTest::ResetResults() {
  last_add_page_result_ = AddPageResult::RESULT_COUNT;
}

void AddPageTaskTest::OnAddPageDone(AddPageResult result) {
  last_add_page_result_ = result;
}

AddPageTask::AddPageTaskCallback AddPageTaskTest::add_page_callback() {
  return base::Bind(&AddPageTaskTest::OnAddPageDone, AsWeakPtr());
}

void AddPageTaskTest::AddPage(const OfflinePageItem& page) {
  auto task = base::MakeUnique<AddPageTask>(store(), page, add_page_callback());
  runner()->RunTask(std::move(task));
}

bool AddPageTaskTest::CheckPageStored(const OfflinePageItem& page) {
  auto stored_page = store_test_util()->GetPageByOfflineId(page.offline_id);
  return stored_page && page == *stored_page;
}

TEST_F(AddPageTaskTest, AddPage) {
  generator()->SetNamespace(kTestNamespace);
  OfflinePageItem page = generator()->CreateItem();
  AddPage(page);

  // Start checking if the page is added into the store.
  EXPECT_TRUE(CheckPageStored(page));
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(AddPageResult::SUCCESS, last_add_page_result());
}

TEST_F(AddPageTaskTest, AddPageWithAllFieldsSet) {
  OfflinePageItem page(kTestUrl1, kTestOfflineId1, kTestClientId1,
                       kTestFilePath, kTestFileSize, base::Time::Now(),
                       kTestOrigin);
  page.title = kTestTitle;
  page.original_url = kTestUrl2;
  page.system_download_id = kTestDownloadId;
  page.file_missing_time = base::Time::Now();
  page.digest = kTestDigest;

  AddPage(page);

  // Start checking if the page is added into the store.
  EXPECT_TRUE(CheckPageStored(page));
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  EXPECT_EQ(AddPageResult::SUCCESS, last_add_page_result());
}

TEST_F(AddPageTaskTest, AddTwoPages) {
  generator()->SetNamespace(kTestNamespace);
  OfflinePageItem page1 = generator()->CreateItem();
  OfflinePageItem page2 = generator()->CreateItem();

  // Adding the first page.
  AddPage(page1);
  EXPECT_EQ(AddPageResult::SUCCESS, last_add_page_result());
  ResetResults();

  // Adding the second page.
  AddPage(page2);
  EXPECT_EQ(AddPageResult::SUCCESS, last_add_page_result());

  // Confirm two pages were added.
  EXPECT_EQ(2LL, store_test_util()->GetPageCount());
  EXPECT_TRUE(CheckPageStored(page1));
  EXPECT_TRUE(CheckPageStored(page2));
}

TEST_F(AddPageTaskTest, AddTwoIdenticalPages) {
  generator()->SetNamespace(kTestNamespace);
  OfflinePageItem page = generator()->CreateItem();

  // Add the page for the first time.
  AddPage(page);
  EXPECT_TRUE(CheckPageStored(page));
  EXPECT_EQ(AddPageResult::SUCCESS, last_add_page_result());
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
  ResetResults();

  // Add the page for the second time, the page should not be added since it
  // already exists in the store.
  AddPage(page);
  EXPECT_EQ(AddPageResult::ALREADY_EXISTS, last_add_page_result());
  EXPECT_EQ(1LL, store_test_util()->GetPageCount());
}

TEST_F(AddPageTaskTest, AddPageWithInvalidStore) {
  generator()->SetNamespace(kTestNamespace);
  OfflinePageItem page = generator()->CreateItem();
  auto task = base::MakeUnique<AddPageTask>(nullptr, page, add_page_callback());
  runner()->RunTask(std::move(task));

  // Start checking if the page is added into the store.
  EXPECT_FALSE(CheckPageStored(page));
  EXPECT_EQ(AddPageResult::STORE_FAILURE, last_add_page_result());
  EXPECT_EQ(0LL, store_test_util()->GetPageCount());
}

}  // namespace offline_pages
