// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/temporary_pages_consistency_check_task.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/offline_page_metadata_store_sql.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

namespace {

struct PageInfo {
  int64_t offline_id;
  base::FilePath file_path;
};

std::vector<PageInfo> GetAllTemporaryPageInfos(
    const std::vector<std::string>& temp_namespaces,
    sql::Connection* db) {
  std::vector<PageInfo> result;

  const char kSql[] =
      "SELECT offline_id, file_path"
      " FROM offlinepages_v1"
      " WHERE client_namespace = ?";

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

std::set<base::FilePath> GetAllArchives(const base::FilePath& archives_dir) {
  std::set<base::FilePath> result;
  base::FileEnumerator file_enumerator(archives_dir, false,
                                       base::FileEnumerator::FILES);
  for (auto archive_path = file_enumerator.Next(); !archive_path.empty();
       archive_path = file_enumerator.Next()) {
    result.insert(archive_path);
  }
  return result;
}

bool DeletePagesByOfflineIds(const std::vector<int64_t>& offline_ids,
                             sql::Connection* db) {
  static const char kSql[] = "DELETE FROM offlinepages_v1 WHERE offline_id = ?";

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

// The method will return false if:
// - An invalid database pointer is provided by the store, which indicates store
//   failure, or
// - The transaction of database operations fails at start or commit, or
// - At least one deletion of an offline page entry failed, or
// - At least one file deletion failed
bool CheckConsistencySync(const base::FilePath& archives_dir,
                          const std::vector<std::string>& namespaces,
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
  for (const auto& page_info : temp_page_infos) {
    // Get pages whose archive files does not exist and delete.
    if (!base::PathExists(page_info.file_path)) {
      offline_ids_to_delete.push_back(page_info.offline_id);
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

  // Delete any files in the temporary archive directory that no longer have
  // associated entries in the database.
  // TODO(romax): https://crbug.com/786240.
  std::set<base::FilePath> archive_paths = GetAllArchives(archives_dir);
  std::vector<base::FilePath> files_to_delete;
  for (const auto& archive_path : archive_paths) {
    if (std::find_if(temp_page_infos.begin(), temp_page_infos.end(),
                     [&archive_path](const PageInfo& page_info) -> bool {
                       return page_info.file_path == archive_path;
                     }) == temp_page_infos.end()) {
      files_to_delete.push_back(archive_path);
    }
  }

  return DeleteFiles(files_to_delete);
}

}  // namespace

TemporaryPagesConsistencyCheckTask::TemporaryPagesConsistencyCheckTask(
    OfflinePageMetadataStoreSQL* store,
    ClientPolicyController* policy_controller,
    const base::FilePath& archives_dir)
    : store_(store),
      policy_controller_(policy_controller),
      archives_dir_(archives_dir),
      weak_ptr_factory_(this) {
  DCHECK(store_);
  DCHECK(policy_controller_);
}

TemporaryPagesConsistencyCheckTask::~TemporaryPagesConsistencyCheckTask() {}

void TemporaryPagesConsistencyCheckTask::Run() {
  std::vector<std::string> temp_namespaces =
      policy_controller_->GetNamespacesRemovedOnCacheReset();
  store_->Execute(
      base::BindOnce(&CheckConsistencySync, archives_dir_, temp_namespaces),
      base::BindOnce(
          &TemporaryPagesConsistencyCheckTask::OnCheckConsistencyDone,
          weak_ptr_factory_.GetWeakPtr()));
}

void TemporaryPagesConsistencyCheckTask::OnCheckConsistencyDone(bool result) {
  // TODO(romax): https://crbug.com/772204. Replace the DVLOG with UMA
  // collecting. If there's a need, introduce more detailed local enums
  // indicating which part failed.
  DVLOG(1) << "TemporaryPagesConsistencyCheck returns with result: " << result;
  TaskComplete();
}

}  // namespace offline_pages
