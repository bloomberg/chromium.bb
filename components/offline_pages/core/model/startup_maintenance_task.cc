// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/startup_maintenance_task.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "components/offline_pages/core/archive_manager.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/offline_page_client_policy.h"
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

std::vector<PageInfo> GetPageInfosByNamespaces(
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

std::set<base::FilePath> GetAllArchives(const base::FilePath& archives_dir) {
  std::set<base::FilePath> result;
  base::FileEnumerator file_enumerator(archives_dir, false,
                                       base::FileEnumerator::FILES,
                                       FILE_PATH_LITERAL("*.mhtml"));
  for (auto archive_path = file_enumerator.Next(); !archive_path.empty();
       archive_path = file_enumerator.Next()) {
    result.insert(archive_path);
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

SyncOperationResult ClearLegacyTempPagesSync(
    sql::Connection* db,
    const std::vector<std::string>& namespaces,
    const base::FilePath& legacy_archives_dir) {
  // One large database transaction that will:
  // 1. Get temporary page infos from the database.
  // 2. Decide which pages to delete (still in legacy archive directory).
  // 3. Delete metadata entries from the database.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return SyncOperationResult::TRANSACTION_BEGIN_ERROR;

  std::vector<PageInfo> temp_page_infos =
      GetPageInfosByNamespaces(namespaces, db);

  std::vector<int64_t> offline_ids_to_delete;
  std::vector<base::FilePath> files_to_delete;
  for (const auto& page_info : temp_page_infos) {
    // Get pages whose archive files are still in the legacy archives directory.
    if (legacy_archives_dir.IsParent(page_info.file_path)) {
      offline_ids_to_delete.push_back(page_info.offline_id);
      files_to_delete.push_back(page_info.file_path);
    }
  }

  // Try to delete the pages by offline ids collected above.
  // If there's any database related errors, the function will return failure,
  // and the database operations will be rolled back since the transaction will
  // not be committed.
  if (!DeletePagesByOfflineIds(offline_ids_to_delete, db))
    return SyncOperationResult::DB_OPERATION_ERROR;

  if (!transaction.Commit())
    return SyncOperationResult::TRANSACTION_COMMIT_ERROR;

  if (!DeleteFiles(files_to_delete))
    return SyncOperationResult::FILE_OPERATION_ERROR;

  return SyncOperationResult::SUCCESS;
}

SyncOperationResult CheckConsistencySync(
    sql::Connection* db,
    const std::vector<std::string>& namespaces,
    const base::FilePath& archives_dir,
    const std::string& prefix_for_uma) {
  // One large database transaction that will:
  // 1. Get page infos by |namespaces| from the database.
  // 2. Decide which pages to delete.
  // 3. Delete metadata entries from the database.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return SyncOperationResult::TRANSACTION_BEGIN_ERROR;

  std::vector<PageInfo> page_infos = GetPageInfosByNamespaces(namespaces, db);

  std::set<base::FilePath> page_info_paths;
  std::vector<int64_t> offline_ids_to_delete;
  for (const auto& page_info : page_infos) {
    // Get pages whose archive files does not exist and delete.
    if (!base::PathExists(page_info.file_path)) {
      offline_ids_to_delete.push_back(page_info.offline_id);
    } else {
      // Extract existing file paths from |page_infos| so that we can do a
      // faster matching later.
      page_info_paths.insert(page_info.file_path);
    }
  }

  if (offline_ids_to_delete.size() > 0) {
    // Try to delete the pages by offline ids collected above. If there's any
    // database related errors, the function will return false, and the database
    // operations will be rolled back since the transaction will not be
    // committed.
    if (!DeletePagesByOfflineIds(offline_ids_to_delete, db))
      return SyncOperationResult::DB_OPERATION_ERROR;
    base::UmaHistogramCounts1M(
        "OfflinePages.ConsistencyCheck." + prefix_for_uma +
            ".PagesMissingArchiveFileCount",
        base::saturated_cast<int32_t>(offline_ids_to_delete.size()));
  }

  if (!transaction.Commit())
    return SyncOperationResult::TRANSACTION_COMMIT_ERROR;

  // Delete any files in the temporary archive directory that no longer have
  // associated entries in the database.
  std::set<base::FilePath> archive_paths = GetAllArchives(archives_dir);
  std::vector<base::FilePath> files_to_delete;
  for (const auto& archive_path : archive_paths) {
    if (page_info_paths.find(archive_path) == page_info_paths.end())
      files_to_delete.push_back(archive_path);
  }

  if (files_to_delete.size() > 0) {
    if (!DeleteFiles(files_to_delete))
      return SyncOperationResult::FILE_OPERATION_ERROR;
    base::UmaHistogramCounts1M("OfflinePages.ConsistencyCheck." +
                                   prefix_for_uma + ".PagesMissingDbEntryCount",
                               static_cast<int32_t>(files_to_delete.size()));
  }

  return SyncOperationResult::SUCCESS;
}

void ReportStorageUsageSync(sql::Connection* db,
                            const std::vector<std::string>& namespaces,
                            ArchiveManager* archive_manager) {
  static const char kSql[] =
      "SELECT sum(file_size) FROM " OFFLINE_PAGES_TABLE_NAME
      " WHERE client_namespace = ?";
  for (const auto& name_space : namespaces) {
    sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
    statement.BindString(0, name_space);
    int size_in_kib = 0;
    while (statement.Step()) {
      size_in_kib = base::saturated_cast<int>(statement.ColumnInt64(0) / 1024);
    }
    base::UmaHistogramCustomCounts(
        "OfflinePages.ClearStoragePreRunUsage2." + name_space, size_in_kib, 1,
        10000000, 50);
  }
}

bool StartupMaintenanceSync(OfflinePageMetadataStoreSQL* store,
                            ArchiveManager* archive_manager,
                            ClientPolicyController* policy_controller,
                            sql::Connection* db) {
  if (!db)
    return false;

  std::vector<std::string> namespaces = policy_controller->GetAllNamespaces();
  std::vector<std::string> temporary_namespaces;
  std::vector<std::string> persistent_namespaces;
  for (const auto& name_space : namespaces) {
    if (!policy_controller->IsRemovedOnCacheReset(name_space))
      persistent_namespaces.push_back(name_space);
    else
      temporary_namespaces.push_back(name_space);
  }

  // Clear temporary pages that are in legacy directory, which is also the
  // directory that serves as the 'private' directory.
  SyncOperationResult result = ClearLegacyTempPagesSync(
      db, temporary_namespaces, archive_manager->GetPrivateArchivesDir());

  // Clear temporary pages in cache directory.
  result = CheckConsistencySync(db, temporary_namespaces,
                                archive_manager->GetTemporaryArchivesDir(),
                                "Temporary" /*prefix_for_uma*/);
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.ConsistencyCheck.Temporary.Result",
                            result, SyncOperationResult::RESULT_COUNT);

  // Clear persistent pages in private directory.
  // TODO(romax): this can be merged with the legacy temporary pages clearing
  // above.
  result = CheckConsistencySync(db, persistent_namespaces,
                                archive_manager->GetPrivateArchivesDir(),
                                "Persistent" /*prefix_for_uma*/);
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.ConsistencyCheck.Persistent.Result",
                            result, SyncOperationResult::RESULT_COUNT);

  // Report storage usage UMA.
  ReportStorageUsageSync(db, namespaces, archive_manager);

  return true;
}

}  // namespace

StartupMaintenanceTask::StartupMaintenanceTask(
    OfflinePageMetadataStoreSQL* store,
    ArchiveManager* archive_manager,
    ClientPolicyController* policy_controller)
    : store_(store),
      archive_manager_(archive_manager),
      policy_controller_(policy_controller),
      weak_ptr_factory_(this) {
  DCHECK(store_);
  DCHECK(archive_manager_);
  DCHECK(policy_controller_);
}

StartupMaintenanceTask::~StartupMaintenanceTask() = default;

void StartupMaintenanceTask::Run() {
  TRACE_EVENT_ASYNC_BEGIN0("offline_pages", "StartupMaintenanceTask running",
                           this);
  store_->Execute(
      base::BindOnce(&StartupMaintenanceSync, store_, archive_manager_,
                     policy_controller_),
      base::BindOnce(&StartupMaintenanceTask::OnStartupMaintenanceDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void StartupMaintenanceTask::OnStartupMaintenanceDone(bool result) {
  TRACE_EVENT_ASYNC_END1("offline_pages", "StartupMaintenanceTask running",
                         this, "result", result);
  TaskComplete();
}

}  // namespace offline_pages
