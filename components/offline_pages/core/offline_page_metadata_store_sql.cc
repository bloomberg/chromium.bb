// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_metadata_store_sql.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

namespace {

// This is a macro instead of a const so that
// it can be used inline in other SQL statements below.
#define OFFLINE_PAGES_TABLE_NAME "offlinepages_v1"

bool CreateOfflinePagesTable(sql::Connection* db) {
  const char kSql[] = "CREATE TABLE IF NOT EXISTS " OFFLINE_PAGES_TABLE_NAME
                      "(offline_id INTEGER PRIMARY KEY NOT NULL,"
                      " creation_time INTEGER NOT NULL,"
                      " file_size INTEGER NOT NULL,"
                      " last_access_time INTEGER NOT NULL,"
                      " access_count INTEGER NOT NULL,"
                      " system_download_id INTEGER NOT NULL DEFAULT 0,"
                      " file_missing_time INTEGER NOT NULL DEFAULT 0,"
                      " upgrade_attempt INTEGER NOT NULL DEFAULT 0,"
                      " client_namespace VARCHAR NOT NULL,"
                      " client_id VARCHAR NOT NULL,"
                      " online_url VARCHAR NOT NULL,"
                      " file_path VARCHAR NOT NULL,"
                      " title VARCHAR NOT NULL DEFAULT '',"
                      " original_url VARCHAR NOT NULL DEFAULT '',"
                      " request_origin VARCHAR NOT NULL DEFAULT '',"
                      " digest VARCHAR NOT NULL DEFAULT ''"
                      ")";
  return db->Execute(kSql);
}

bool UpgradeWithQuery(sql::Connection* db, const char* upgrade_sql) {
  if (!db->Execute("ALTER TABLE " OFFLINE_PAGES_TABLE_NAME
                   " RENAME TO temp_" OFFLINE_PAGES_TABLE_NAME)) {
    return false;
  }
  if (!CreateOfflinePagesTable(db))
    return false;
  if (!db->Execute(upgrade_sql))
    return false;
  if (!db->Execute("DROP TABLE IF EXISTS temp_" OFFLINE_PAGES_TABLE_NAME))
    return false;
  return true;
}

bool UpgradeFrom52(sql::Connection* db) {
  const char kSql[] =
      "INSERT INTO " OFFLINE_PAGES_TABLE_NAME
      " (offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, "
      "online_url, file_path) "
      "SELECT "
      "offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, "
      "online_url, file_path "
      "FROM temp_" OFFLINE_PAGES_TABLE_NAME;
  return UpgradeWithQuery(db, kSql);
}

bool UpgradeFrom53(sql::Connection* db) {
  const char kSql[] =
      "INSERT INTO " OFFLINE_PAGES_TABLE_NAME
      " (offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, online_url, "
      "file_path) "
      "SELECT "
      "offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, online_url, "
      "file_path "
      "FROM temp_" OFFLINE_PAGES_TABLE_NAME;
  return UpgradeWithQuery(db, kSql);
}

bool UpgradeFrom54(sql::Connection* db) {
  const char kSql[] =
      "INSERT INTO " OFFLINE_PAGES_TABLE_NAME
      " (offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, online_url, "
      "file_path, title) "
      "SELECT "
      "offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, online_url, "
      "file_path, title "
      "FROM temp_" OFFLINE_PAGES_TABLE_NAME;
  return UpgradeWithQuery(db, kSql);
}

bool UpgradeFrom55(sql::Connection* db) {
  const char kSql[] =
      "INSERT INTO " OFFLINE_PAGES_TABLE_NAME
      " (offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, online_url, "
      "file_path, title) "
      "SELECT "
      "offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, online_url, "
      "file_path, title "
      "FROM temp_" OFFLINE_PAGES_TABLE_NAME;
  return UpgradeWithQuery(db, kSql);
}

bool UpgradeFrom56(sql::Connection* db) {
  const char kSql[] =
      "INSERT INTO " OFFLINE_PAGES_TABLE_NAME
      " (offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, online_url, "
      "file_path, title, original_url) "
      "SELECT "
      "offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, online_url, "
      "file_path, title, original_url "
      "FROM temp_" OFFLINE_PAGES_TABLE_NAME;
  return UpgradeWithQuery(db, kSql);
}

bool UpgradeFrom57(sql::Connection* db) {
  const char kSql[] =
      "INSERT INTO " OFFLINE_PAGES_TABLE_NAME
      " (offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, online_url, "
      "file_path, title, original_url) "
      "SELECT "
      "offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, online_url, "
      "file_path, title, original_url "
      "FROM temp_" OFFLINE_PAGES_TABLE_NAME;
  return UpgradeWithQuery(db, kSql);
}

bool UpgradeFrom61(sql::Connection* db) {
  const char kSql[] =
      "INSERT INTO " OFFLINE_PAGES_TABLE_NAME
      " (offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, online_url, "
      "file_path, title, original_url, request_origin) "
      "SELECT "
      "offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, online_url, "
      "file_path, title, original_url, request_origin "
      "FROM temp_" OFFLINE_PAGES_TABLE_NAME;
  return UpgradeWithQuery(db, kSql);
}

bool CreateSchema(sql::Connection* db) {
  // If you create a transaction but don't Commit() it is automatically
  // rolled back by its destructor when it falls out of scope.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  if (!db->DoesTableExist(OFFLINE_PAGES_TABLE_NAME)) {
    if (!CreateOfflinePagesTable(db))
      return false;
  }

  // Upgrade section. Details are described in the header file.
  if (!db->DoesColumnExist(OFFLINE_PAGES_TABLE_NAME, "expiration_time") &&
      !db->DoesColumnExist(OFFLINE_PAGES_TABLE_NAME, "title")) {
    if (!UpgradeFrom52(db))
      return false;
  } else if (!db->DoesColumnExist(OFFLINE_PAGES_TABLE_NAME, "title")) {
    if (!UpgradeFrom53(db))
      return false;
  } else if (db->DoesColumnExist(OFFLINE_PAGES_TABLE_NAME, "offline_url")) {
    if (!UpgradeFrom54(db))
      return false;
  } else if (!db->DoesColumnExist(OFFLINE_PAGES_TABLE_NAME, "original_url")) {
    if (!UpgradeFrom55(db))
      return false;
  } else if (db->DoesColumnExist(OFFLINE_PAGES_TABLE_NAME, "expiration_time")) {
    if (!UpgradeFrom56(db))
      return false;
  } else if (!db->DoesColumnExist(OFFLINE_PAGES_TABLE_NAME, "request_origin")) {
    if (!UpgradeFrom57(db))
      return false;
  } else if (!db->DoesColumnExist(OFFLINE_PAGES_TABLE_NAME, "digest")) {
    if (!UpgradeFrom61(db))
      return false;
  }

  // This would be a great place to add indices when we need them.
  return transaction.Commit();
}

bool DeleteByOfflineId(sql::Connection* db, int64_t offline_id) {
  static const char kSql[] =
      "DELETE FROM " OFFLINE_PAGES_TABLE_NAME " WHERE offline_id=?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, offline_id);
  return statement.Run();
}

// Create an offline page item from a SQL result.  Expects complete rows with
// all columns present.
OfflinePageItem MakeOfflinePageItem(sql::Statement* statement) {
  int64_t id = statement->ColumnInt64(0);
  base::Time creation_time =
      store_utils::FromDatabaseTime(statement->ColumnInt64(1));
  int64_t file_size = statement->ColumnInt64(2);
  base::Time last_access_time =
      store_utils::FromDatabaseTime(statement->ColumnInt64(3));
  int access_count = statement->ColumnInt(4);
  int64_t system_download_id = statement->ColumnInt64(5);
  base::Time file_missing_time =
      store_utils::FromDatabaseTime(statement->ColumnInt64(6));
  int upgrade_attempt = statement->ColumnInt(7);
  ClientId client_id(statement->ColumnString(8), statement->ColumnString(9));
  GURL url(statement->ColumnString(10));
  base::FilePath path(
      store_utils::FromDatabaseFilePath(statement->ColumnString(11)));
  base::string16 title = statement->ColumnString16(12);
  GURL original_url(statement->ColumnString(13));
  std::string request_origin = statement->ColumnString(14);
  std::string digest = statement->ColumnString(15);

  OfflinePageItem item(url, id, client_id, path, file_size, creation_time);
  item.last_access_time = last_access_time;
  item.access_count = access_count;
  item.title = title;
  item.original_url = original_url;
  item.request_origin = request_origin;
  item.system_download_id = system_download_id;
  item.file_missing_time = file_missing_time;
  item.upgrade_attempt = upgrade_attempt;
  item.digest = digest;
  return item;
}

ItemActionStatus AddOfflinePageSync(const OfflinePageItem& item,
                                    sql::Connection* db) {
  if (!db)
    return ItemActionStatus::STORE_ERROR;

  // Using 'INSERT OR FAIL' or 'INSERT OR ABORT' in the query below causes debug
  // builds to DLOG.
  const char kSql[] =
      "INSERT OR IGNORE INTO " OFFLINE_PAGES_TABLE_NAME
      " (offline_id, online_url, client_namespace, client_id, file_path, "
      "file_size, creation_time, last_access_time, access_count, "
      "title, original_url, request_origin, system_download_id, "
      "file_missing_time, upgrade_attempt, digest)"
      " VALUES "
      " (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, item.offline_id);
  statement.BindString(1, item.url.spec());
  statement.BindString(2, item.client_id.name_space);
  statement.BindString(3, item.client_id.id);
  statement.BindString(4, store_utils::ToDatabaseFilePath(item.file_path));
  statement.BindInt64(5, item.file_size);
  statement.BindInt64(6, store_utils::ToDatabaseTime(item.creation_time));
  statement.BindInt64(7, store_utils::ToDatabaseTime(item.last_access_time));
  statement.BindInt(8, item.access_count);
  statement.BindString16(9, item.title);
  statement.BindString(10, item.original_url.spec());
  statement.BindString(11, item.request_origin);
  statement.BindInt64(12, item.system_download_id);
  statement.BindInt64(13, store_utils::ToDatabaseTime(item.file_missing_time));
  statement.BindInt(14, item.upgrade_attempt);
  statement.BindString(15, item.digest);

  if (!statement.Run())
    return ItemActionStatus::STORE_ERROR;
  if (db->GetLastChangeCount() == 0)
    return ItemActionStatus::ALREADY_EXISTS;
  return ItemActionStatus::SUCCESS;
}

bool Update(sql::Connection* db, const OfflinePageItem& item) {
  const char kSql[] =
      "UPDATE OR IGNORE " OFFLINE_PAGES_TABLE_NAME
      " SET online_url = ?, client_namespace = ?, client_id = ?, file_path = ?,"
      " file_size = ?, creation_time = ?, last_access_time = ?,"
      " access_count = ?, title = ?, original_url = ?, request_origin = ?,"
      " system_download_id = ?, file_missing_time = ?, upgrade_attempt = ?,"
      " digest = ?"
      " WHERE offline_id = ?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, item.url.spec());
  statement.BindString(1, item.client_id.name_space);
  statement.BindString(2, item.client_id.id);
  statement.BindString(3, store_utils::ToDatabaseFilePath(item.file_path));
  statement.BindInt64(4, item.file_size);
  statement.BindInt64(5, store_utils::ToDatabaseTime(item.creation_time));
  statement.BindInt64(6, store_utils::ToDatabaseTime(item.last_access_time));
  statement.BindInt(7, item.access_count);
  statement.BindString16(8, item.title);
  statement.BindString(9, item.original_url.spec());
  statement.BindString(10, item.request_origin);
  statement.BindInt64(11, item.system_download_id);
  statement.BindInt64(12, store_utils::ToDatabaseTime(item.file_missing_time));
  statement.BindInt(13, item.upgrade_attempt);
  statement.BindString(14, item.digest);
  statement.BindInt64(15, item.offline_id);
  return statement.Run() && db->GetLastChangeCount() > 0;
}

bool PrepareDirectory(const base::FilePath& path) {
  base::File::Error error = base::File::FILE_OK;
  if (!base::DirectoryExists(path.DirName())) {
    if (!base::CreateDirectoryAndGetError(path.DirName(), &error)) {
      LOG(ERROR) << "Failed to create offline pages db directory: "
                 << base::File::ErrorToString(error);
      return false;
    }
  }

  UMA_HISTOGRAM_ENUMERATION("OfflinePages.SQLStorage.CreateDirectoryResult",
                            -error, -base::File::FILE_ERROR_MAX);

  return true;
}

bool InitDatabase(sql::Connection* db,
                  const base::FilePath& path,
                  bool in_memory) {
  db->set_page_size(4096);
  db->set_cache_size(500);
  db->set_histogram_tag("OfflinePageMetadata");
  db->set_exclusive_locking();

  if (!in_memory && !PrepareDirectory(path))
    return false;

  bool open_db_result = false;
  if (in_memory)
    open_db_result = db->OpenInMemory();
  else
    open_db_result = db->Open(path);

  if (!open_db_result) {
    LOG(ERROR) << "Failed to open database, in memory: " << in_memory;
    return false;
  }
  db->Preload();

  return CreateSchema(db);
}

void RecordLoadResult(OfflinePageMetadataStore::LoadStatus status,
                      int32_t page_count) {
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.LoadStatus", status,
                            OfflinePageMetadataStore::LOAD_STATUS_COUNT);
  if (status == OfflinePageMetadataStore::LOAD_SUCCEEDED) {
    UMA_HISTOGRAM_COUNTS("OfflinePages.SavedPageCount", page_count);
  } else {
    DVLOG(1) << "Offline pages database loading failed: " << status;
  }
}

// This method does a simple check whether DB was initialized properly.
// It is only relevant for direct call to Initialize.
bool CheckDBLoadedSync(sql::Connection* db) {
  return db != nullptr;
}

bool GetPageByOfflineIdSync(sql::Connection* db,
                            int64_t offline_id,
                            OfflinePageItem* item) {
  const char kSql[] =
      "SELECT * FROM " OFFLINE_PAGES_TABLE_NAME " WHERE offline_id = ?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, offline_id);

  if (statement.Step()) {
    *item = MakeOfflinePageItem(&statement);
    return true;
  }

  return false;
}

std::vector<OfflinePageItem> GetOfflinePagesSync(sql::Connection* db) {
  std::vector<OfflinePageItem> result;

  if (!db)
    return result;

  const char kSql[] = "SELECT * FROM " OFFLINE_PAGES_TABLE_NAME;
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));

  while (statement.Step())
    result.push_back(MakeOfflinePageItem(&statement));

  if (statement.Succeeded()) {
    RecordLoadResult(OfflinePageMetadataStore::LOAD_SUCCEEDED, result.size());
  } else {
    result.clear();
    RecordLoadResult(OfflinePageMetadataStore::STORE_LOAD_FAILED, 0);
  }

  return result;
}

void GetOfflinePagesResultWrapper(
    const OfflinePageMetadataStore::LoadCallback& load_callback,
    std::vector<OfflinePageItem> result) {
  load_callback.Run(result);
}

std::unique_ptr<OfflinePagesUpdateResult> BuildUpdateResultForIds(
    StoreState store_state,
    const std::vector<int64_t>& offline_ids,
    ItemActionStatus action_status) {
  std::unique_ptr<OfflinePagesUpdateResult> result(
      new OfflinePagesUpdateResult(store_state));
  for (const auto& offline_id : offline_ids)
    result->item_statuses.push_back(std::make_pair(offline_id, action_status));
  return result;
}

std::unique_ptr<OfflinePagesUpdateResult> BuildUpdateResultForPages(
    StoreState store_state,
    const std::vector<OfflinePageItem>& pages,
    ItemActionStatus action_status) {
  std::unique_ptr<OfflinePagesUpdateResult> result(
      new OfflinePagesUpdateResult(store_state));
  for (const auto& page : pages)
    result->item_statuses.push_back(
        std::make_pair(page.offline_id, action_status));
  return result;
}

std::unique_ptr<OfflinePagesUpdateResult> UpdateOfflinePagesSync(
    const std::vector<OfflinePageItem>& pages,
    sql::Connection* db) {
  if (!db) {
    return BuildUpdateResultForPages(StoreState::NOT_LOADED, pages,
                                     ItemActionStatus::STORE_ERROR);
  }

  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    return BuildUpdateResultForPages(StoreState::LOADED, pages,
                                     ItemActionStatus::STORE_ERROR);
  }

  std::unique_ptr<OfflinePagesUpdateResult> result(
      new OfflinePagesUpdateResult(StoreState::LOADED));

  for (const auto& page : pages) {
    if (Update(db, page)) {
      result->updated_items.push_back(page);
      result->item_statuses.push_back(
          std::make_pair(page.offline_id, ItemActionStatus::SUCCESS));
    } else {
      result->item_statuses.push_back(
          std::make_pair(page.offline_id, ItemActionStatus::NOT_FOUND));
    }
  }

  if (!transaction.Commit()) {
    return BuildUpdateResultForPages(StoreState::LOADED, pages,
                                     ItemActionStatus::STORE_ERROR);
  }

  return result;
}

std::unique_ptr<OfflinePagesUpdateResult> RemoveOfflinePagesSync(
    const std::vector<int64_t>& offline_ids,
    sql::Connection* db) {
  if (!db) {
    return BuildUpdateResultForIds(StoreState::NOT_LOADED, offline_ids,
                                   ItemActionStatus::STORE_ERROR);
  }

  if (offline_ids.empty()) {
    return BuildUpdateResultForIds(StoreState::LOADED, offline_ids,
                                   ItemActionStatus::NOT_FOUND);
  }

  // If you create a transaction but don't Commit() it is automatically
  // rolled back by its destructor when it falls out of scope.
  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    return BuildUpdateResultForIds(StoreState::LOADED, offline_ids,
                                   ItemActionStatus::STORE_ERROR);
  }

  std::unique_ptr<OfflinePagesUpdateResult> result(
      new OfflinePagesUpdateResult(StoreState::LOADED));
  for (int64_t offline_id : offline_ids) {
    OfflinePageItem page;
    ItemActionStatus status;
    if (!GetPageByOfflineIdSync(db, offline_id, &page)) {
      status = ItemActionStatus::NOT_FOUND;
    } else if (!DeleteByOfflineId(db, offline_id)) {
      status = ItemActionStatus::STORE_ERROR;
    } else {
      status = ItemActionStatus::SUCCESS;
      result->updated_items.push_back(page);
    }

    result->item_statuses.push_back(std::make_pair(offline_id, status));
  }

  if (!transaction.Commit()) {
    return BuildUpdateResultForIds(StoreState::LOADED, offline_ids,
                                   ItemActionStatus::STORE_ERROR);
  }

  return result;
}

}  // anonymous namespace

OfflinePageMetadataStoreSQL::OfflinePageMetadataStoreSQL(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : background_task_runner_(std::move(background_task_runner)),
      in_memory_(true),
      state_(StoreState::NOT_LOADED),
      weak_ptr_factory_(this) {}

OfflinePageMetadataStoreSQL::OfflinePageMetadataStoreSQL(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const base::FilePath& path)
    : background_task_runner_(std::move(background_task_runner)),
      in_memory_(false),
      db_file_path_(path.AppendASCII("OfflinePages.db")),
      state_(StoreState::NOT_LOADED),
      weak_ptr_factory_(this) {}

OfflinePageMetadataStoreSQL::~OfflinePageMetadataStoreSQL() {
  if (db_.get() &&
      !background_task_runner_->DeleteSoon(FROM_HERE, db_.release())) {
    DLOG(WARNING) << "SQL database will not be deleted.";
  }
}

// TODO(fgorski): Remove this method once model is split into tasks.
// This method is only kept until the whole OfflinePageModelImpl is running
// implemented using TaskQueue. A separate initialize step will not be necessary
// after that.
void OfflinePageMetadataStoreSQL::Initialize(
    const InitializeCallback& callback) {
  Execute(base::BindOnce(&CheckDBLoadedSync), base::BindOnce(callback));
}

void OfflinePageMetadataStoreSQL::GetOfflinePages(
    const LoadCallback& callback) {
  Execute(base::BindOnce(&GetOfflinePagesSync),
          base::BindOnce(&GetOfflinePagesResultWrapper, callback));
}

void OfflinePageMetadataStoreSQL::AddOfflinePage(
    const OfflinePageItem& offline_page,
    const AddCallback& callback) {
  Execute(base::BindOnce(&AddOfflinePageSync, offline_page),
          base::BindOnce(callback));
}

void OfflinePageMetadataStoreSQL::UpdateOfflinePages(
    const std::vector<OfflinePageItem>& pages,
    const UpdateCallback& callback) {
  Execute(base::BindOnce(&UpdateOfflinePagesSync, pages),
          base::BindOnce(callback));
}

void OfflinePageMetadataStoreSQL::RemoveOfflinePages(
    const std::vector<int64_t>& offline_ids,
    const UpdateCallback& callback) {
  Execute(base::BindOnce(&RemoveOfflinePagesSync, offline_ids),
          base::BindOnce(callback));
}

// No-op implementation. This database does not reset.
// TODO(dimich): Observe UMA and decide if this database has to be erased.
// Note is potentially contains user-saved data.
void OfflinePageMetadataStoreSQL::Reset(const ResetCallback& callback) {
  bool success = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback, success));
  return;
}

StoreState OfflinePageMetadataStoreSQL::state() const {
  return state_;
}

void OfflinePageMetadataStoreSQL::SetStateForTesting(StoreState state,
                                                     bool reset_db) {
  state_ = state;
  if (reset_db)
    db_.reset(nullptr);
}

void OfflinePageMetadataStoreSQL::InitializeInternal(
    base::OnceClosure pending_command) {
  // TODO(fgorski): Set state to initializing/loading.
  DCHECK_EQ(state_, StoreState::NOT_LOADED);

  db_.reset(new sql::Connection());
  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&InitDatabase, db_.get(), db_file_path_, in_memory_),
      base::BindOnce(&OfflinePageMetadataStoreSQL::OnInitializeInternalDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(pending_command)));
}

void OfflinePageMetadataStoreSQL::OnInitializeInternalDone(
    base::OnceClosure pending_command,
    bool success) {
  // TODO(fgorski): DCHECK initialization is in progress, once we have a
  // relevant value for the store state.
  state_ = success ? StoreState::LOADED : StoreState::FAILED_LOADING;

  CHECK(!pending_command.is_null());
  std::move(pending_command).Run();

  if (state_ == StoreState::FAILED_LOADING)
    state_ = StoreState::NOT_LOADED;
}

}  // namespace offline_pages
