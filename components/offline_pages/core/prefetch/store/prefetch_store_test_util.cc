// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "url/gurl.h"

namespace offline_pages {
namespace {

int CountPrefetchItemsSync(sql::Connection* db) {
  // Not starting transaction as this is a single read.
  static const char kSql[] = "SELECT COUNT(offline_id) FROM prefetch_items";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  if (statement.Step())
    return statement.ColumnInt(0);

  return kStoreCommandFailed;
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

  return kStoreCommandFailed;
}

}  // namespace

PrefetchStoreTestUtil::PrefetchStoreTestUtil() {}

PrefetchStoreTestUtil::~PrefetchStoreTestUtil() = default;

void PrefetchStoreTestUtil::BuildStore() {
  if (!temp_directory_.CreateUniqueTempDir())
    DVLOG(1) << "temp_directory_ not created";

  store_.reset(new PrefetchStore(base::ThreadTaskRunnerHandle::Get(),
                                 temp_directory_.GetPath()));
}

void PrefetchStoreTestUtil::BuildStoreInMemory() {
  store_.reset(new PrefetchStore(base::ThreadTaskRunnerHandle::Get()));
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

void PrefetchStoreTestUtil::CountPrefetchItems(
    PrefetchStore::ResultCallback<int> result_callback) {
  store_->Execute(base::BindOnce(&CountPrefetchItemsSync),
                  std::move(result_callback));
}

void PrefetchStoreTestUtil::ZombifyPrefetchItem(
    const std::string& name_space,
    const GURL& url,
    PrefetchStore::ResultCallback<int> result_callback) {
  store_->Execute(base::BindOnce(&UpdateItemsStateSync, name_space, url.spec(),
                                 PrefetchItemState::ZOMBIE),
                  std::move(result_callback));
}
}  // namespace offline_pages
