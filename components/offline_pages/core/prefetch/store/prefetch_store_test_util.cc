// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_utils.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "url/gurl.h"

namespace offline_pages {
namespace {

int64_t InsertPrefetchItemSync(const PrefetchItem& item, sql::Connection* db) {
  static const char kSql[] =
      "INSERT INTO prefetch_items"
      " (offline_id, state, request_archive_attempt_count,"
      "  archive_body_length, creation_time, freshness_time,"
      "  error_code, guid, client_namespace, client_id, requested_url,"
      "  final_archived_url, operation_name, archive_body_name)"
      " VALUES"
      " (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, item.offline_id);
  statement.BindInt(1, static_cast<int>(item.state));
  statement.BindInt(2, item.request_archive_attempt_count);
  statement.BindInt64(3, item.archive_body_length);
  statement.BindInt64(4, item.creation_time.ToInternalValue());
  statement.BindInt64(5, item.freshness_time.ToInternalValue());
  statement.BindInt(6, static_cast<int>(item.error_code));
  statement.BindString(7, item.guid);
  statement.BindString(8, item.client_id.name_space);
  statement.BindString(9, item.client_id.id);
  statement.BindString(10, item.url.spec());
  statement.BindString(11, item.final_archived_url.spec());
  statement.BindString(12, item.operation_name);
  statement.BindString(13, item.archive_body_name);

  if (!statement.Run())
    return PrefetchStoreTestUtil::kStoreCommandFailed;

  return item.offline_id;
}

int CountPrefetchItemsSync(sql::Connection* db) {
  // Not starting transaction as this is a single read.
  static const char kSql[] = "SELECT COUNT(offline_id) FROM prefetch_items";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  if (statement.Step())
    return statement.ColumnInt(0);

  return PrefetchStoreTestUtil::kStoreCommandFailed;
}

int UpdateItemsStateSync(const std::string& name_space,
                         const std::string& url,
                         PrefetchItemState state,
                         sql::Connection* db) {
  static const char kSql[] =
      "UPDATE prefetch_items"
      " SET state = ?"
      " WHERE client_namespace = ? AND requested_url = ?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(state));
  statement.BindString(1, name_space);
  statement.BindString(2, url);
  if (statement.Run())
    return db->GetLastChangeCount();

  return PrefetchStoreTestUtil::kStoreCommandFailed;
}

std::unique_ptr<PrefetchItem> GetPrefetchItemSync(int64_t offline_id,
                                                  sql::Connection* db) {
  static const char kSql[] =
      "SELECT "
      "  offline_id, state, request_archive_attempt_count,"
      "  archive_body_length, creation_time, freshness_time,"
      "  error_code, guid, client_namespace, client_id, requested_url,"
      "  final_archived_url, operation_name, archive_body_name"
      " FROM prefetch_items WHERE offline_id = ?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, offline_id);

  if (!statement.Step())
    return nullptr;

  auto item = base::MakeUnique<PrefetchItem>();
  item->offline_id = statement.ColumnInt64(0);
  item->state = static_cast<PrefetchItemState>(statement.ColumnInt(1));
  item->request_archive_attempt_count = statement.ColumnInt(2);
  item->archive_body_length = statement.ColumnInt64(3);
  item->creation_time = base::Time::FromInternalValue(statement.ColumnInt64(4));
  item->freshness_time =
      base::Time::FromInternalValue(statement.ColumnInt64(5));
  item->error_code = static_cast<PrefetchItemErrorCode>(statement.ColumnInt(6));
  item->guid = statement.ColumnString(7);
  item->client_id.name_space = statement.ColumnString(8);
  item->client_id.id = statement.ColumnString(9);
  item->url = GURL(statement.ColumnString(10));
  item->final_archived_url = GURL(statement.ColumnString(11));
  item->operation_name = statement.ColumnString(12);
  item->archive_body_name = statement.ColumnString(13);

  return item;
}
}  // namespace

PrefetchStoreTestUtil::PrefetchStoreTestUtil(
    scoped_refptr<base::TestSimpleTaskRunner> task_runner)
    : task_runner_(task_runner) {}

PrefetchStoreTestUtil::~PrefetchStoreTestUtil() = default;

void PrefetchStoreTestUtil::BuildStore() {
  if (!temp_directory_.CreateUniqueTempDir())
    DVLOG(1) << "temp_directory_ not created";

  store_.reset(new PrefetchStore(task_runner_, temp_directory_.GetPath()));
}

void PrefetchStoreTestUtil::BuildStoreInMemory() {
  store_.reset(new PrefetchStore(task_runner_));
}

std::unique_ptr<PrefetchStore> PrefetchStoreTestUtil::ReleaseStore() {
  return std::move(store_);
}

void PrefetchStoreTestUtil::DeleteStore() {
  store_.reset();
  if (temp_directory_.IsValid()) {
    if (!temp_directory_.Delete())
      DVLOG(1) << "temp_directory_ not created";
  }
}

void PrefetchStoreTestUtil::RunUntilIdle() {
  task_runner_->RunUntilIdle();
}

int PrefetchStoreTestUtil::ZombifyPrefetchItems(const std::string& name_space,
                                                const GURL& url) {
  int count = 0;
  store_->Execute(
      base::BindOnce(&UpdateItemsStateSync, name_space, url.spec(),
                     PrefetchItemState::ZOMBIE),
      base::BindOnce([](int* result, int count) { *result = count; }, &count));
  RunUntilIdle();
  return count;
}

int64_t PrefetchStoreTestUtil::InsertPrefetchItem(const PrefetchItem& item) {
  int64_t offline_id = kStoreCommandFailed;
  store_->Execute(
      base::BindOnce(&InsertPrefetchItemSync, item),
      base::BindOnce([](int64_t* result, int64_t id) { *result = id; },
                     &offline_id));
  RunUntilIdle();
  return offline_id;
}

int PrefetchStoreTestUtil::CountPrefetchItems() {
  int count = 0;
  store_->Execute(
      base::BindOnce(&CountPrefetchItemsSync),
      base::BindOnce([](int* result, int count) { *result = count; }, &count));
  RunUntilIdle();
  return count;
}

std::unique_ptr<PrefetchItem> PrefetchStoreTestUtil::GetPrefetchItem(
    int64_t offline_id) {
  std::unique_ptr<PrefetchItem> item;
  store_->Execute(base::BindOnce(&GetPrefetchItemSync, offline_id),
                  base::BindOnce(
                      [](std::unique_ptr<PrefetchItem>* item_alias,
                         std::unique_ptr<PrefetchItem> result) {
                        *item_alias = std::move(result);
                      },
                      &item));
  RunUntilIdle();
  return item;
}

}  // namespace offline_pages
