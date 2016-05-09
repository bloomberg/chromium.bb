// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_storage_manager.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "components/offline_pages/client_policy_controller.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_model.h"
#include "testing/gtest/include/gtest/gtest.h"

using DeletePageResult = offline_pages::OfflinePageModel::DeletePageResult;

namespace offline_pages {

namespace {
const char kTestClientNamespace[] = "CLIENT_NAMESPACE";
const GURL kTestUrl("http://example.com");
const GURL kTestUrl2("http://other.page.com");
const base::FilePath::CharType kFilePath[] = FILE_PATH_LITERAL("TEST_FILEPATH");
const ClientId kTestClientId1(kTestClientNamespace, "1234");
const ClientId kTestClientId2(kTestClientNamespace, "5678");
const int64_t kTestFileSize = 876543LL;
const int64_t kOfflineId = 1234LL;
const int64_t kOfflineId2 = 2345LL;
}  // namespace

class OfflinePageTestModel : public OfflinePageModel {
 public:
  OfflinePageTestModel() : policy_controller_(new ClientPolicyController()) {
    // Manually adding pages for the fake model.
    // Only the first page is expired.
    pages_.clear();
    pages_.push_back(OfflinePageItem(kTestUrl, kOfflineId, kTestClientId1,
                                     base::FilePath(kFilePath), kTestFileSize));
    pages_.push_back(OfflinePageItem(kTestUrl2, kOfflineId2, kTestClientId2,
                                     base::FilePath(kFilePath), kTestFileSize));
    base::Time now = base::Time::Now();
    pages_[0].last_access_time = now - base::TimeDelta::FromDays(10);
    pages_[1].last_access_time = now;
  }

  void GetAllPages(const MultipleOfflinePageItemCallback& callback) override {
    callback.Run(pages_);
  }

  void DeletePagesByOfflineId(const std::vector<int64_t>& offline_ids,
                              const DeletePageCallback& callback) override {
    callback.Run(DeletePageResult::SUCCESS);
  }

  ClientPolicyController* GetPolicyController() override {
    return policy_controller_.get();
  }

  bool is_loaded() const override { return true; }

 private:
  std::vector<OfflinePageItem> pages_;

  std::unique_ptr<ClientPolicyController> policy_controller_;
};

class OfflinePageStorageManagerTest : public testing::Test {
 public:
  OfflinePageStorageManagerTest();
  OfflinePageStorageManager* manager() { return manager_.get(); }
  void OnPagesCleared(int pages_cleared_count, DeletePageResult result);

  // testing::Test
  void SetUp() override;
  void TearDown() override;

  int last_cleared_page_count() const { return last_cleared_page_count_; }
  DeletePageResult last_delete_page_result() const {
    return last_delete_page_result_;
  }

 private:
  std::unique_ptr<OfflinePageStorageManager> manager_;
  std::unique_ptr<OfflinePageModel> model_;

  int last_cleared_page_count_;
  DeletePageResult last_delete_page_result_;
};

OfflinePageStorageManagerTest::OfflinePageStorageManagerTest()
    : last_cleared_page_count_(0),
      last_delete_page_result_(DeletePageResult::SUCCESS) {}

void OfflinePageStorageManagerTest::SetUp() {
  model_.reset(new OfflinePageTestModel());
  manager_.reset(new OfflinePageStorageManager(model_.get()));
}

void OfflinePageStorageManagerTest::TearDown() {
  manager_.reset();
  model_.reset();
}

void OfflinePageStorageManagerTest::OnPagesCleared(int pages_cleared_count,
                                                   DeletePageResult result) {
  last_cleared_page_count_ = pages_cleared_count;
  last_delete_page_result_ = result;
}

TEST_F(OfflinePageStorageManagerTest, TestClearPages) {
  manager()->ClearPagesIfNeeded(base::Bind(
      &OfflinePageStorageManagerTest::OnPagesCleared, base::Unretained(this)));
  EXPECT_EQ(1, last_cleared_page_count());
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
}

}  // namespace offline_pages
