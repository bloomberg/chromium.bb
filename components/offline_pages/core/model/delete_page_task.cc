// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/delete_page_task.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "build/build_config.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/offline_page_metadata_store_sql.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

using DeletePageTaskResult = DeletePageTask::DeletePageTaskResult;

namespace {

#define OFFLINE_PAGES_TABLE_NAME "offlinepages_v1"

// A wrapper of DeletedPageInfo to include |file_path| in order to be used
// through the deletion process. This is implementation detail and it will be
// used to create OfflinePageModel::DeletedPageInfo that are passed through
// callback.
struct DeletedPageInfoWrapper {
  DeletedPageInfoWrapper();
  DeletedPageInfoWrapper(const DeletedPageInfoWrapper& other);
  int64_t offline_id;
  ClientId client_id;
  base::FilePath file_path;
  std::string request_origin;
};

DeletedPageInfoWrapper::DeletedPageInfoWrapper() = default;
DeletedPageInfoWrapper::DeletedPageInfoWrapper(
    const DeletedPageInfoWrapper& other) = default;

bool DeleteArchiveSync(const base::FilePath& file_path) {
  // Delete the file only, |false| for recursive.
  return base::DeleteFile(file_path, false);
}

// Deletes a page from the store by |offline_id|.
bool DeletePageEntryByOfflineIdSync(sql::Connection* db, int64_t offline_id) {
  static const char kSql[] =
      "DELETE FROM " OFFLINE_PAGES_TABLE_NAME " WHERE offline_id = ?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, offline_id);
  return statement.Run();
}

// Deletes pages by DeletedPageInfoWrapper. This will return a
// DeletePageTaskResult which contains the infos of the deleted pages (which are
// successfully deleted from the disk and the store) and a DeletePageResult.
// For each DeletedPageInfoWrapper to be deleted, the deletion will delete the
// archive file first, then database entry, in order to avoid the potential
// issue of leaving archive files behind (and they may be imported later).
// Since the database entry will only be deleted while the associated archive
// file is deleted successfully, there will be no such issue.
DeletePageTaskResult DeletePagesByDeletedPageInfoWrappersSync(
    sql::Connection* db,
    const std::vector<DeletedPageInfoWrapper>& info_wrappers) {
  std::vector<OfflinePageModel::DeletedPageInfo> deleted_page_infos;

  // If there's no page to delete, return an empty list with SUCCESS.
  if (info_wrappers.size() == 0)
    return DeletePageTaskResult(DeletePageResult::SUCCESS, deleted_page_infos);

  bool any_archive_deleted = false;
  for (const auto& info_wrapper : info_wrappers) {
    if (DeleteArchiveSync(info_wrapper.file_path)) {
      any_archive_deleted = true;
      if (DeletePageEntryByOfflineIdSync(db, info_wrapper.offline_id))
        deleted_page_infos.emplace_back(info_wrapper.offline_id,
                                        info_wrapper.client_id,
                                        info_wrapper.request_origin);
    }
  }
  // If there're no files deleted, return DEVICE_FAILURE.
  if (!any_archive_deleted)
    return DeletePageTaskResult(DeletePageResult::DEVICE_FAILURE,
                                deleted_page_infos);

  return DeletePageTaskResult(DeletePageResult::SUCCESS, deleted_page_infos);
}

// Gets the page info for |offline_id|, returning in |info_wrapper|. Returns
// false if there's no record for |offline_id|.
bool GetDeletedPageInfoWrapperByOfflineIdSync(
    sql::Connection* db,
    int64_t offline_id,
    DeletedPageInfoWrapper* info_wrapper) {
  static const char kSql[] =
      "SELECT client_namespace, client_id, file_path, request_origin"
      " FROM " OFFLINE_PAGES_TABLE_NAME " WHERE offline_id = ?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, offline_id);

  if (statement.Step()) {
    info_wrapper->offline_id = offline_id;
    info_wrapper->client_id.name_space = statement.ColumnString(0);
    info_wrapper->client_id.id = statement.ColumnString(1);
    info_wrapper->file_path =
        store_utils::FromDatabaseFilePath(statement.ColumnString(2));
    info_wrapper->request_origin = statement.ColumnString(3);
    return true;
  }
  return false;
}

DeletePageTaskResult DeletePagesByOfflineIdsSync(
    const std::vector<int64_t>& offline_ids,
    sql::Connection* db) {
  if (!db)
    return DeletePageTaskResult(DeletePageResult::STORE_FAILURE, {});
  // TODO(romax): Make sure there's not regression to treat deleting empty list
  // of offline ids as successful (if it's not what we currently do).
  if (offline_ids.empty())
    return DeletePageTaskResult(DeletePageResult::SUCCESS, {});

  // If you create a transaction but dont Commit() it is automatically
  // rolled back by its destructor when it falls out of scope.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return DeletePageTaskResult(DeletePageResult::STORE_FAILURE, {});

  std::vector<DeletedPageInfoWrapper> infos;
  for (int64_t offline_id : offline_ids) {
    DeletedPageInfoWrapper info;
    if (GetDeletedPageInfoWrapperByOfflineIdSync(db, offline_id, &info))
      infos.push_back(info);
  }
  DeletePageTaskResult result =
      DeletePagesByDeletedPageInfoWrappersSync(db, infos);

  if (!transaction.Commit())
    return DeletePageTaskResult(DeletePageResult::STORE_FAILURE, {});
  return result;
}

// Gets page infos for |client_id|, returning a vector of
// DeletedPageInfoWrappers because ClientId can refer to multiple pages.
std::vector<DeletedPageInfoWrapper> GetDeletedPageInfoWrappersByClientIdSync(
    sql::Connection* db,
    ClientId client_id) {
  std::vector<DeletedPageInfoWrapper> info_wrappers;
  static const char kSql[] =
      "SELECT offline_id, file_path, request_origin"
      " FROM " OFFLINE_PAGES_TABLE_NAME
      " WHERE client_namespace = ? AND client_id = ?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, client_id.name_space);
  statement.BindString(1, client_id.id);

  while (statement.Step()) {
    DeletedPageInfoWrapper info_wrapper;
    info_wrapper.client_id = client_id;
    info_wrapper.offline_id = statement.ColumnInt64(0);
    info_wrapper.file_path =
        store_utils::FromDatabaseFilePath(statement.ColumnString(1));
    info_wrapper.request_origin = statement.ColumnString(2);
    info_wrappers.push_back(info_wrapper);
  }
  return info_wrappers;
}

DeletePageTaskResult DeletePagesByClientIdsSync(
    const std::vector<ClientId> client_ids,
    sql::Connection* db) {
  std::vector<DeletedPageInfoWrapper> infos;

  if (!db)
    return DeletePageTaskResult(DeletePageResult::STORE_FAILURE, {});
  // TODO(romax): Make sure there's not regression to treat deleting empty list
  // of offline ids as successful (if it's not what we currently do).
  if (client_ids.empty())
    return DeletePageTaskResult(DeletePageResult::SUCCESS, {});

  // If you create a transaction but dont Commit() it is automatically
  // rolled back by its destructor when it falls out of scope.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return DeletePageTaskResult(DeletePageResult::STORE_FAILURE, {});

  for (ClientId client_id : client_ids) {
    std::vector<DeletedPageInfoWrapper> temp_infos =
        GetDeletedPageInfoWrappersByClientIdSync(db, client_id);
    infos.insert(infos.end(), temp_infos.begin(), temp_infos.end());
  }

  DeletePageTaskResult result =
      DeletePagesByDeletedPageInfoWrappersSync(db, infos);

  if (!transaction.Commit())
    return DeletePageTaskResult(DeletePageResult::STORE_FAILURE, {});
  return result;
}

// Gets the page information of pages that are within the provided temporary
// namespaces and satisfy the provided URL predicate.
std::vector<DeletedPageInfoWrapper>
GetCachedDeletedPageInfoWrappersByUrlPredicateSync(
    sql::Connection* db,
    const std::vector<std::string>& temp_namespaces,
    const UrlPredicate& url_predicate) {
  std::vector<DeletedPageInfoWrapper> info_wrappers;
  static const char kSql[] =
      "SELECT offline_id, client_namespace, client_id, file_path,"
      " request_origin, online_url"
      " FROM " OFFLINE_PAGES_TABLE_NAME " WHERE client_namespace = ?";

  for (const auto& temp_namespace : temp_namespaces) {
    sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
    statement.BindString(0, temp_namespace);

    while (statement.Step()) {
      if (!url_predicate.Run(GURL(statement.ColumnString(5))))
        continue;
      DeletedPageInfoWrapper info_wrapper;
      info_wrapper.offline_id = statement.ColumnInt64(0);
      info_wrapper.client_id.name_space = statement.ColumnString(1);
      info_wrapper.client_id.id = statement.ColumnString(2);
      info_wrapper.file_path =
          store_utils::FromDatabaseFilePath(statement.ColumnString(3));
      info_wrapper.request_origin = statement.ColumnString(4);
      info_wrappers.push_back(info_wrapper);
    }
  }
  return info_wrappers;
}

DeletePageTaskResult DeleteCachedPagesByUrlPredicateSync(
    const std::vector<std::string>& namespaces,
    const UrlPredicate& predicate,
    sql::Connection* db) {
  if (!db)
    return DeletePageTaskResult(DeletePageResult::STORE_FAILURE, {});

  // If you create a transaction but dont Commit() it is automatically
  // rolled back by its destructor when it falls out of scope.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return DeletePageTaskResult(DeletePageResult::STORE_FAILURE, {});

  const std::vector<DeletedPageInfoWrapper>& infos =
      GetCachedDeletedPageInfoWrappersByUrlPredicateSync(db, namespaces,
                                                         predicate);
  DeletePageTaskResult result =
      DeletePagesByDeletedPageInfoWrappersSync(db, infos);

  if (!transaction.Commit())
    return DeletePageTaskResult(DeletePageResult::STORE_FAILURE, {});
  return result;
}

}  // namespace

// DeletePageTaskResult implementations.
DeletePageTaskResult::DeletePageTaskResult() = default;
DeletePageTaskResult::DeletePageTaskResult(
    DeletePageResult result,
    const std::vector<OfflinePageModel::DeletedPageInfo>& infos)
    : result(result), infos(infos) {}
DeletePageTaskResult::DeletePageTaskResult(const DeletePageTaskResult& other) =
    default;
DeletePageTaskResult::~DeletePageTaskResult() = default;

// static
std::unique_ptr<DeletePageTask> DeletePageTask::CreateTaskMatchingOfflineIds(
    OfflinePageMetadataStoreSQL* store,
    const std::vector<int64_t>& offline_ids,
    DeletePageTask::DeletePageTaskCallback callback) {
  return std::unique_ptr<DeletePageTask>(new DeletePageTask(
      store, base::BindOnce(&DeletePagesByOfflineIdsSync, offline_ids),
      std::move(callback)));
}

// static
std::unique_ptr<DeletePageTask> DeletePageTask::CreateTaskMatchingClientIds(
    OfflinePageMetadataStoreSQL* store,
    const std::vector<ClientId>& client_ids,
    DeletePageTask::DeletePageTaskCallback callback) {
  return std::unique_ptr<DeletePageTask>(new DeletePageTask(
      store, base::BindOnce(&DeletePagesByClientIdsSync, client_ids),
      std::move(callback)));
}

// static
std::unique_ptr<DeletePageTask>
DeletePageTask::CreateTaskMatchingUrlPredicateForCachedPages(
    OfflinePageMetadataStoreSQL* store,
    ClientPolicyController* policy_controller,
    const UrlPredicate& predicate,
    DeletePageTask::DeletePageTaskCallback callback) {
  std::vector<std::string> temp_namespaces =
      policy_controller->GetNamespacesRemovedOnCacheReset();
  return std::unique_ptr<DeletePageTask>(
      new DeletePageTask(store,
                         base::BindOnce(&DeleteCachedPagesByUrlPredicateSync,
                                        temp_namespaces, predicate),
                         std::move(callback)));
}

DeletePageTask::DeletePageTask(OfflinePageMetadataStoreSQL* store,
                               DeleteFunction func,
                               DeletePageTaskCallback callback)
    : store_(store),
      func_(std::move(func)),
      callback_(std::move(callback)),
      weak_ptr_factory_(this) {}

DeletePageTask::~DeletePageTask() {}

void DeletePageTask::Run() {
  store_->Execute(std::move(func_),
                  base::BindOnce(&DeletePageTask::OnDeletePageDone,
                                 weak_ptr_factory_.GetWeakPtr()));
}

void DeletePageTask::OnDeletePageDone(DeletePageTaskResult result) {
  std::move(callback_).Run(result.result, result.infos);
  TaskComplete();
}

}  // namespace offline_pages
