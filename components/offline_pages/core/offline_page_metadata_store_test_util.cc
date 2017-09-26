// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_metadata_store_test_util.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "components/offline_pages/core/model/get_pages_task.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

int64_t GetPageCountSync(sql::Connection* db) {
  const char kSql[] = "SELECT count(*) FROM offlinepages_v1";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  if (statement.Step()) {
    return statement.ColumnInt64(0);
  }
  return 0UL;
}

}  // namespace

OfflinePageMetadataStoreTestUtil::OfflinePageMetadataStoreTestUtil(
    scoped_refptr<base::TestSimpleTaskRunner> task_runner)
    : task_runner_(task_runner) {}

OfflinePageMetadataStoreTestUtil::~OfflinePageMetadataStoreTestUtil() {}

void OfflinePageMetadataStoreTestUtil::BuildStore() {
  if (!temp_directory_.CreateUniqueTempDir()) {
    DVLOG(1) << "temp_directory_ not created";
    return;
  }

  store_.reset(
      new OfflinePageMetadataStoreSQL(task_runner_, temp_directory_.GetPath()));
}

void OfflinePageMetadataStoreTestUtil::BuildStoreInMemory() {
  store_.reset(new OfflinePageMetadataStoreSQL(task_runner_));
}

void OfflinePageMetadataStoreTestUtil::DeleteStore() {
  store_.reset();
  task_runner_->RunUntilIdle();
}

std::unique_ptr<OfflinePageMetadataStoreSQL>
OfflinePageMetadataStoreTestUtil::ReleaseStore() {
  return std::move(store_);
}

void OfflinePageMetadataStoreTestUtil::InsertItem(const OfflinePageItem& page) {
  ItemActionStatus status;
  store_->AddOfflinePage(
      page, base::Bind([](ItemActionStatus* out_status,
                          ItemActionStatus status) { *out_status = status; },
                       &status));
  task_runner_->RunUntilIdle();
  EXPECT_EQ(ItemActionStatus::SUCCESS, status);
}

int64_t OfflinePageMetadataStoreTestUtil::GetPageCount() {
  int64_t page_count = 0;
  store_->Execute(base::BindOnce(&GetPageCountSync),
                  base::BindOnce([](int64_t* out_count,
                                    int64_t count) { *out_count = count; },
                                 &page_count));
  task_runner_->RunUntilIdle();
  return page_count;
}

OfflinePageItem OfflinePageMetadataStoreTestUtil::GetPageByOfflineId(
    int64_t offline_id) {
  OfflinePageItem out_page;
  auto task = GetPagesTask::CreateTaskMatchingOfflineId(
      store(),
      base::Bind(
          [](OfflinePageItem* out_page, const OfflinePageItem* page) {
            if (page)
              *out_page = *page;
          },
          &out_page),
      offline_id);
  task->Run();
  task_runner_->RunUntilIdle();
  return out_page;
}

}  // namespace offline_pages
