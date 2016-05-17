// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_storage_manager.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "components/offline_pages/client_policy_controller.h"
#include "components/offline_pages/offline_page_item.h"
#include "components/offline_pages/offline_page_storage_manager.h"
#include "components/offline_pages/offline_page_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
const char kBookmarkNamespace[] = "bookmark";
const char kLastNNamespace[] = "last_n";
const GURL kTestUrl("http://example.com");
const base::FilePath::CharType kFilePath[] = FILE_PATH_LITERAL("TEST_FILEPATH");
const int64_t kTestFileSize = 876543LL;

enum TestOptions {
  DEFAULT = 1 << 0,
  DELETE_FAILED = 1 << 1,
};

struct PageSettings {
  std::string name_space;
  int fresh_pages_count;
  int expired_pages_count;
};
}  // namespace

class StorageManagerTestClient : public OfflinePageStorageManager::Client {
 public:
  StorageManagerTestClient(std::vector<PageSettings> page_settings,
                           base::SimpleTestClock* clock,
                           TestOptions options = TestOptions::DEFAULT)
      : policy_controller_(new ClientPolicyController()),
        clock_(clock),
        options_(options),
        next_offline_id_(0) {
    for (const auto& setting : page_settings)
      AddPages(setting);
  }

  ~StorageManagerTestClient() override;

  void GetAllPages(const MultipleOfflinePageItemCallback& callback) override {
    callback.Run(pages_);
  }

  void DeletePagesByOfflineId(const std::vector<int64_t>& offline_ids,
                              const DeletePageCallback& callback) override {
    if (options_ & TestOptions::DELETE_FAILED) {
      callback.Run(DeletePageResult::STORE_FAILURE);
    } else {
      callback.Run(DeletePageResult::SUCCESS);
    }
  }

  base::SimpleTestClock* clock() { return clock_; }

 private:
  void AddPages(const PageSettings& setting);

  std::vector<OfflinePageItem> pages_;

  std::unique_ptr<ClientPolicyController> policy_controller_;

  base::SimpleTestClock* clock_;

  TestOptions options_;

  int64_t next_offline_id_;
};

StorageManagerTestClient::~StorageManagerTestClient() {}

void StorageManagerTestClient::AddPages(const PageSettings& setting) {
  std::string name_space = setting.name_space;
  int fresh_pages_count = setting.fresh_pages_count;
  int expired_pages_count = setting.expired_pages_count;
  base::Time now = clock()->Now();
  // Fresh pages.
  for (int i = 0; i < fresh_pages_count; i++) {
    OfflinePageItem page =
        OfflinePageItem(kTestUrl, next_offline_id_,
                        ClientId(name_space, std::to_string(next_offline_id_)),
                        base::FilePath(kFilePath), kTestFileSize);
    next_offline_id_++;
    page.last_access_time = now;
    pages_.push_back(page);
  }
  // Expired pages.
  for (int i = 0; i < expired_pages_count; i++) {
    OfflinePageItem page =
        OfflinePageItem(kTestUrl, next_offline_id_,
                        ClientId(name_space, std::to_string(next_offline_id_)),
                        base::FilePath(kFilePath), kTestFileSize);
    next_offline_id_++;
    page.last_access_time = now -
                            policy_controller_->GetPolicy(name_space)
                                .lifetime_policy.expiration_period;
    pages_.push_back(page);
  }
}

class OfflinePageStorageManagerTest : public testing::Test {
 public:
  OfflinePageStorageManagerTest();
  OfflinePageStorageManager* manager() { return manager_.get(); }
  OfflinePageStorageManager::Client* client() { return client_.get(); }
  ClientPolicyController* policy_controller() {
    return policy_controller_.get();
  }
  void OnPagesCleared(int pages_cleared_count, DeletePageResult result);
  void Initialize(const std::vector<PageSettings>& settings,
                  TestOptions options = TestOptions::DEFAULT);

  // testing::Test
  void TearDown() override;

  base::SimpleTestClock* clock() { return clock_; }
  int last_cleared_page_count() const { return last_cleared_page_count_; }
  DeletePageResult last_delete_page_result() const {
    return last_delete_page_result_;
  }

 private:
  std::unique_ptr<OfflinePageStorageManager> manager_;
  std::unique_ptr<OfflinePageStorageManager::Client> client_;
  std::unique_ptr<ClientPolicyController> policy_controller_;

  base::SimpleTestClock* clock_;

  int last_cleared_page_count_;
  DeletePageResult last_delete_page_result_;
};

OfflinePageStorageManagerTest::OfflinePageStorageManagerTest()
    : policy_controller_(new ClientPolicyController()),
      last_cleared_page_count_(0),
      last_delete_page_result_(DeletePageResult::SUCCESS) {}

void OfflinePageStorageManagerTest::Initialize(
    const std::vector<PageSettings>& page_settings,
    TestOptions options) {
  std::unique_ptr<base::SimpleTestClock> clock(new base::SimpleTestClock());
  clock_ = clock.get();
  clock_->SetNow(base::Time::Now());
  client_.reset(new StorageManagerTestClient(page_settings, clock_, options));
  manager_.reset(new OfflinePageStorageManager(client(), policy_controller()));
  manager_->SetClockForTesting(std::move(clock));
}

void OfflinePageStorageManagerTest::TearDown() {
  manager_.reset();
  client_.reset();
}

void OfflinePageStorageManagerTest::OnPagesCleared(int pages_cleared_count,
                                                   DeletePageResult result) {
  last_cleared_page_count_ = pages_cleared_count;
  last_delete_page_result_ = result;
}

TEST_F(OfflinePageStorageManagerTest, TestClearPagesLessThanLimit) {
  Initialize(std::vector<PageSettings>(
      {{kBookmarkNamespace, 1, 1}, {kLastNNamespace, 1, 1}}));
  clock()->Advance(base::TimeDelta::FromMinutes(30));
  manager()->ClearPagesIfNeeded(base::Bind(
      &OfflinePageStorageManagerTest::OnPagesCleared, base::Unretained(this)));
  EXPECT_EQ(2, last_cleared_page_count());
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
}

TEST_F(OfflinePageStorageManagerTest, TestClearPagesMoreThanLimit) {
  Initialize(std::vector<PageSettings>(
      {{kBookmarkNamespace, 10, 15}, {kLastNNamespace, 5, 30}}));
  clock()->Advance(base::TimeDelta::FromMinutes(30));
  manager()->ClearPagesIfNeeded(base::Bind(
      &OfflinePageStorageManagerTest::OnPagesCleared, base::Unretained(this)));
  EXPECT_EQ(45, last_cleared_page_count());
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
}

TEST_F(OfflinePageStorageManagerTest, TestClearPagesMoreFreshPages) {
  Initialize(std::vector<PageSettings>(
      {{kBookmarkNamespace, 30, 0}, {kLastNNamespace, 100, 1}}));
  clock()->Advance(base::TimeDelta::FromMinutes(30));
  manager()->ClearPagesIfNeeded(base::Bind(
      &OfflinePageStorageManagerTest::OnPagesCleared, base::Unretained(this)));
  int last_n_page_limit = policy_controller()
                              ->GetPolicy(kLastNNamespace)
                              .lifetime_policy.page_limit;
  EXPECT_EQ(1 + (100 - last_n_page_limit), last_cleared_page_count());
  EXPECT_EQ(DeletePageResult::SUCCESS, last_delete_page_result());
}
}  // namespace offline_pages
