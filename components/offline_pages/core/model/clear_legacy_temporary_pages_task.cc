// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/clear_legacy_temporary_pages_task.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/offline_page_metadata_store_sql.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

namespace {

#define OFFLINE_PAGES_TABLE_NAME "offlinepages_v1"

struct PageInfo {
  int64_t offline_id;
  base::FilePath file_path;
};

std::vector<PageInfo> GetAllTemporaryPageInfos(
    const std::vector<std::string>& temp_namespaces,
    sql::Connection* db) {
  std::vector<PageInfo> result;

  static const char kSql[] =
      "SELECT offline_id, file_path"
      " FROM " OFFLINE_PAGES_TABLE_NAME " WHERE client_namespace = ?";

  for (const auto& temp_namespace : temp_namespaces) {
    sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
    statement.BindString(0, temp_namespace);
    while (statement.Step()) {
      result.push_back(
          {statement.ColumnInt64(0),
           store_utils::FromDatabaseFilePath(statement.ColumnString(1))});
    }
  }

  return result;
}

bool DeletePagesByOfflineIds(const std::vector<int64_t>& offline_ids,
                             sql::Connection* db) {
  static const char kSql[] =
      "DELETE FROM " OFFLINE_PAGES_TABLE_NAME " WHERE offline_id = ?";

  for (const auto& offline_id : offline_ids) {
    sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
    statement.BindInt64(0, offline_id);
    if (!statement.Run())
      return false;
  }
  return true;
}

bool DeleteFiles(const std::vector<base::FilePath>& file_paths) {
  bool result = true;
  for (const auto& file_path : file_paths)
    result = base::DeleteFile(file_path, false) && result;
  return result;
}

bool ClearLegacyTempPagesSync(const std::vector<std::string>& namespaces,
                              const base::FilePath& legacy_archives_dir,
                              sql::Connection* db) {
  if (!db)
    return false;

  // One large database transaction that will:
  // 1. Get temporary page infos from the database.
  // 2. Decide which pages to delete.
  // 3. Delete metadata entries from the database.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  auto temp_page_infos = GetAllTemporaryPageInfos(namespaces, db);

  std::vector<int64_t> offline_ids_to_delete;
  std::vector<base::FilePath> files_to_delete;
  for (const auto& page_info : temp_page_infos) {
    // Get pages whose archive files are still in the legacy archives directory.
    if (legacy_archives_dir.IsParent(page_info.file_path)) {
      offline_ids_to_delete.push_back(page_info.offline_id);
      files_to_delete.push_back(page_info.file_path);
      continue;
    }
  }

  // Try to delete the pages by offline ids collected above.
  // If there's any database related errors, the function will return false,
  // and the database operations will be rolled back since the transaction will
  // not be committed.
  if (!DeletePagesByOfflineIds(offline_ids_to_delete, db))
    return false;

  if (!transaction.Commit())
    return false;

  return DeleteFiles(files_to_delete);
}

}  // namespace

ClearLegacyTemporaryPagesTask::ClearLegacyTemporaryPagesTask(
    OfflinePageMetadataStoreSQL* store,
    ClientPolicyController* policy_controller,
    const base::FilePath& legacy_archives_dir)
    : store_(store),
      policy_controller_(policy_controller),
      legacy_archives_dir_(legacy_archives_dir),
      weak_ptr_factory_(this) {}

ClearLegacyTemporaryPagesTask::~ClearLegacyTemporaryPagesTask() {}

void ClearLegacyTemporaryPagesTask::Run() {
  std::vector<std::string> temp_namespaces =
      policy_controller_->GetNamespacesRemovedOnCacheReset();
  store_->Execute(
      base::BindOnce(&ClearLegacyTempPagesSync, temp_namespaces,
                     legacy_archives_dir_),
      base::BindOnce(
          &ClearLegacyTemporaryPagesTask::OnClearLegacyTemporaryPagesDone,
          weak_ptr_factory_.GetWeakPtr()));
}

void ClearLegacyTemporaryPagesTask::OnClearLegacyTemporaryPagesDone(
    bool result) {
  // TODO(romax): https://crbug.com/772204. Replace the DVLOG with UMA
  // collecting. If there's a need, introduce more detailed local enums
  // indicating which part failed.
  DVLOG(1) << "ClearLegacyTemporaryPagesTask returns with result: " << result;
  TaskComplete();
}

}  // namespace offline_pages
