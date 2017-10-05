// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/mark_page_accessed_task.h"

#include "base/bind.h"
#include "components/offline_pages/core/offline_page_metadata_store_sql.h"
#include "components/offline_pages/core/offline_time_utils.h"
#include "sql/connection.h"
#include "sql/statement.h"

namespace offline_pages {

namespace {

bool MarkPageAccessedSync(const base::Time& last_access_time,
                          int64_t offline_id,
                          sql::Connection* db) {
  const char kSql[] =
      "UPDATE OR IGNORE offlinepages_v1"
      " SET last_access_time = ?, access_count = access_count + 1"
      " WHERE offline_id = ?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, ToDatabaseTime(last_access_time));
  statement.BindInt64(1, offline_id);
  return statement.Run();
}

}  // namespace

MarkPageAccessedTask::MarkPageAccessedTask(OfflinePageMetadataStoreSQL* store,
                                           int64_t offline_id,
                                           const base::Time& access_time)
    : store_(store), offline_id_(offline_id), access_time_(access_time) {}

MarkPageAccessedTask::~MarkPageAccessedTask(){};

void MarkPageAccessedTask::Run() {
  store_->Execute(
      base::BindOnce(&MarkPageAccessedSync, access_time_, offline_id_),
      base::BindOnce([](bool success) {}));
}

}  // namespace offline_pages
