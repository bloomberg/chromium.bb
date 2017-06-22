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
                      " client_namespace VARCHAR NOT NULL,"
                      " client_id VARCHAR NOT NULL,"
                      " online_url VARCHAR NOT NULL,"
                      " file_path VARCHAR NOT NULL,"
                      " title VARCHAR NOT NULL DEFAULT '',"
                      " original_url VARCHAR NOT NULL DEFAULT '',"
                      " request_origin VARCHAR NOT NULL DEFAULT ''"
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

base::FilePath GetPathFromUTF8String(const std::string& path_string) {
#if defined(OS_POSIX)
  return base::FilePath(path_string);
#elif defined(OS_WIN)
  return base::FilePath(base::UTF8ToWide(path_string));
#else
#error Unknown OS
#endif
}

std::string GetUTF8StringFromPath(const base::FilePath& path) {
#if defined(OS_POSIX)
  return path.value();
#elif defined(OS_WIN)
  return base::WideToUTF8(path.value());
#else
#error Unknown OS
#endif
}

// Create an offline page item from a SQL result.  Expects complete rows with
// all columns present.
OfflinePageItem MakeOfflinePageItem(sql::Statement* statement) {
  int64_t id = statement->ColumnInt64(0);
  base::Time creation_time =
      base::Time::FromInternalValue(statement->ColumnInt64(1));
  int64_t file_size = statement->ColumnInt64(2);
  base::Time last_access_time =
      base::Time::FromInternalValue(statement->ColumnInt64(3));
  int access_count = statement->ColumnInt(4);
  ClientId client_id(statement->ColumnString(5), statement->ColumnString(6));
  GURL url(statement->ColumnString(7));
  base::FilePath path(GetPathFromUTF8String(statement->ColumnString(8)));
  base::string16 title = statement->ColumnString16(9);
  GURL original_url(statement->ColumnString(10));
  std::string request_origin = statement->ColumnString(11);

  OfflinePageItem item(url, id, client_id, path, file_size, creation_time);
  item.last_access_time = last_access_time;
  item.access_count = access_count;
  item.title = title;
  item.original_url = original_url;
  item.request_origin = request_origin;
  return item;
}

ItemActionStatus Insert(sql::Connection* db, const OfflinePageItem& item) {
  // Using 'INSERT OR FAIL' or 'INSERT OR ABORT' in the query below causes debug
  // builds to DLOG.
  const char kSql[] =
      "INSERT OR IGNORE INTO " OFFLINE_PAGES_TABLE_NAME
      " (offline_id, online_url, client_namespace, client_id, file_path, "
      "file_size, creation_time, last_access_time, access_count, "
      "title, original_url, request_origin)"
      " VALUES "
      " (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, item.offline_id);
  statement.BindString(1, item.url.spec());
  statement.BindString(2, item.client_id.name_space);
  statement.BindString(3, item.client_id.id);
  statement.BindString(4, GetUTF8StringFromPath(item.file_path));
  statement.BindInt64(5, item.file_size);
  statement.BindInt64(6, item.creation_time.ToInternalValue());
  statement.BindInt64(7, item.last_access_time.ToInternalValue());
  statement.BindInt(8, item.access_count);
  statement.BindString16(9, item.title);
  statement.BindString(10, item.original_url.spec());
  statement.BindString(11, item.request_origin);
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
      " access_count = ?, title = ?, original_url = ?, request_origin = ?"
      " WHERE offline_id = ?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindString(0, item.url.spec());
  statement.BindString(1, item.client_id.name_space);
  statement.BindString(2, item.client_id.id);
  statement.BindString(3, GetUTF8StringFromPath(item.file_path));
  statement.BindInt64(4, item.file_size);
  statement.BindInt64(5, item.creation_time.ToInternalValue());
  statement.BindInt64(6, item.last_access_time.ToInternalValue());
  statement.BindInt(7, item.access_count);
  statement.BindString16(8, item.title);
  statement.BindString(9, item.original_url.spec());
  statement.BindString(10, item.request_origin);
  statement.BindInt64(11, item.offline_id);
  return statement.Run() && db->GetLastChangeCount() > 0;
}

bool InitDatabase(sql::Connection* db, base::FilePath path) {
  db->set_page_size(4096);
  db->set_cache_size(500);
  db->set_histogram_tag("OfflinePageMetadata");
  db->set_exclusive_locking();

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

  if (!db->Open(path)) {
    LOG(ERROR) << "Failed to open database";
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

void OpenConnectionSync(
    sql::Connection* db,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const base::FilePath& path,
    const base::Callback<void(StoreState)>& set_state_callback,
    const OfflinePageMetadataStore::InitializeCallback& callback) {
  bool success = InitDatabase(db, path);
  // First set the state of the DB, then report to the caller.
  // Sequence of these callbacks is important.
  runner->PostTask(FROM_HERE, base::Bind(set_state_callback,
                                         success ? StoreState::LOADED
                                                 : StoreState::FAILED_LOADING));
  runner->PostTask(FROM_HERE, base::Bind(callback, success));
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

void GetOfflinePagesSync(
    sql::Connection* db,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const base::Callback<void(StoreState)>& set_state_callback,
    const OfflinePageMetadataStore::LoadCallback& callback) {
  const char kSql[] = "SELECT * FROM " OFFLINE_PAGES_TABLE_NAME;

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));

  std::vector<OfflinePageItem> result;
  while (statement.Step())
    result.push_back(MakeOfflinePageItem(&statement));

  if (statement.Succeeded()) {
    RecordLoadResult(OfflinePageMetadataStore::LOAD_SUCCEEDED, result.size());
  } else {
    result.clear();
    // Zombify store. If we were unable to read from it, avoid consistency
    // check and other potentially destructive operations.
    // Note this callback should be posted back to the original thread before
    // the main callback got the GetOfflinePages caller. Sequence of these
    // callbacks is important.
    runner->PostTask(
        FROM_HERE, base::Bind(set_state_callback, StoreState::FAILED_LOADING));

    RecordLoadResult(OfflinePageMetadataStore::STORE_LOAD_FAILED, 0);
  }
  runner->PostTask(FROM_HERE, base::Bind(callback, result));
}

void AddOfflinePageSync(sql::Connection* db,
                        scoped_refptr<base::SingleThreadTaskRunner> runner,
                        const OfflinePageItem& offline_page,
                        const OfflinePageMetadataStore::AddCallback& callback) {
  ItemActionStatus status = Insert(db, offline_page);
  runner->PostTask(FROM_HERE, base::Bind(callback, status));
}

void PostStoreUpdateResultForIds(
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    StoreState store_state,
    const std::vector<int64_t>& offline_ids,
    ItemActionStatus action_status,
    const OfflinePageMetadataStore::UpdateCallback& callback) {
  std::unique_ptr<OfflinePagesUpdateResult> result(
      new OfflinePagesUpdateResult(store_state));
  for (const auto& offline_id : offline_ids)
    result->item_statuses.push_back(std::make_pair(offline_id, action_status));
  runner->PostTask(FROM_HERE, base::Bind(callback, base::Passed(&result)));
}

void PostStoreErrorForAllPages(
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const std::vector<OfflinePageItem>& pages,
    const OfflinePageMetadataStore::UpdateCallback& callback) {
  std::vector<int64_t> offline_ids;
  for (const auto& page : pages)
    offline_ids.push_back(page.offline_id);
  PostStoreUpdateResultForIds(runner, StoreState::LOADED, offline_ids,
                              ItemActionStatus::STORE_ERROR, callback);
}

void PostStoreErrorForAllIds(
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const std::vector<int64_t>& offline_ids,
    const OfflinePageMetadataStore::UpdateCallback& callback) {
  PostStoreUpdateResultForIds(runner, StoreState::LOADED, offline_ids,
                              ItemActionStatus::STORE_ERROR, callback);
}

void UpdateOfflinePagesSync(
    sql::Connection* db,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const std::vector<OfflinePageItem>& pages,
    const OfflinePageMetadataStore::UpdateCallback& callback) {
  std::unique_ptr<OfflinePagesUpdateResult> result(
      new OfflinePagesUpdateResult(StoreState::LOADED));

  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    PostStoreErrorForAllPages(runner, pages, callback);
    return;
  }

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
    PostStoreErrorForAllPages(runner, pages, callback);
    return;
  }
  runner->PostTask(FROM_HERE, base::Bind(callback, base::Passed(&result)));
}

void RemoveOfflinePagesSync(
    const std::vector<int64_t>& offline_ids,
    sql::Connection* db,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    const OfflinePageMetadataStore::UpdateCallback& callback) {
  std::unique_ptr<OfflinePagesUpdateResult> result(
      new OfflinePagesUpdateResult(StoreState::LOADED));

  // If you create a transaction but don't Commit() it is automatically
  // rolled back by its destructor when it falls out of scope.
  sql::Transaction transaction(db);
  if (!transaction.Begin()) {
    PostStoreErrorForAllIds(runner, offline_ids, callback);
    return;
  }

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
    PostStoreErrorForAllIds(runner, offline_ids, callback);
    return;
  }

  runner->PostTask(FROM_HERE, base::Bind(callback, base::Passed(&result)));
}

}  // anonymous namespace

OfflinePageMetadataStoreSQL::OfflinePageMetadataStoreSQL(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const base::FilePath& path)
    : background_task_runner_(std::move(background_task_runner)),
      db_file_path_(path.AppendASCII("OfflinePages.db")),
      state_(StoreState::NOT_LOADED),
      weak_ptr_factory_(this) {
}

OfflinePageMetadataStoreSQL::~OfflinePageMetadataStoreSQL() {
  if (db_.get() &&
      !background_task_runner_->DeleteSoon(FROM_HERE, db_.release())) {
    DLOG(WARNING) << "SQL database will not be deleted.";
  }
}

void OfflinePageMetadataStoreSQL::Initialize(
    const InitializeCallback& callback) {
  DCHECK(!db_);
  db_.reset(new sql::Connection());
  background_task_runner_->PostTask(
      FROM_HERE, base::Bind(&OpenConnectionSync, db_.get(),
                            base::ThreadTaskRunnerHandle::Get(), db_file_path_,
                            base::Bind(&OfflinePageMetadataStoreSQL::SetState,
                                       weak_ptr_factory_.GetWeakPtr()),
                            callback));
}

void OfflinePageMetadataStoreSQL::GetOfflinePages(
    const LoadCallback& callback) {
  if (!CheckDb()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, std::vector<OfflinePageItem>()));
    return;
  }

  background_task_runner_->PostTask(
      FROM_HERE, base::Bind(&GetOfflinePagesSync, db_.get(),
                            base::ThreadTaskRunnerHandle::Get(),
                            base::Bind(&OfflinePageMetadataStoreSQL::SetState,
                                       weak_ptr_factory_.GetWeakPtr()),
                            callback));
}

void OfflinePageMetadataStoreSQL::AddOfflinePage(
    const OfflinePageItem& offline_page,
    const AddCallback& callback) {
  if (!CheckDb()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, ItemActionStatus::STORE_ERROR));
    return;
  }

  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AddOfflinePageSync, db_.get(),
                 base::ThreadTaskRunnerHandle::Get(), offline_page, callback));
}

void OfflinePageMetadataStoreSQL::UpdateOfflinePages(
    const std::vector<OfflinePageItem>& pages,
    const UpdateCallback& callback) {
  if (!CheckDb()) {
    PostStoreErrorForAllPages(base::ThreadTaskRunnerHandle::Get(), pages,
                              callback);
    return;
  }

  background_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UpdateOfflinePagesSync, db_.get(),
                 base::ThreadTaskRunnerHandle::Get(), pages, callback));
}

void OfflinePageMetadataStoreSQL::RemoveOfflinePages(
    const std::vector<int64_t>& offline_ids,
    const UpdateCallback& callback) {
  if (!CheckDb()) {
    PostStoreErrorForAllIds(base::ThreadTaskRunnerHandle::Get(), offline_ids,
                            callback);
    return;
  }

  if (offline_ids.empty()) {
    // Nothing to do, but post a callback instead of calling directly
    // to preserve the async style behavior to prevent bugs.
    PostStoreUpdateResultForIds(
        base::ThreadTaskRunnerHandle::Get(), state(), offline_ids,
        ItemActionStatus::NOT_FOUND /* will be ignored */, callback);
    return;
  }

  background_task_runner_->PostTask(
      FROM_HERE, base::Bind(&RemoveOfflinePagesSync, offline_ids, db_.get(),
                            base::ThreadTaskRunnerHandle::Get(), callback));
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

void OfflinePageMetadataStoreSQL::SetState(StoreState state) {
  DCHECK(db_.get());
  DCHECK(state == StoreState::LOADED || state == StoreState::FAILED_LOADING);
  state_ = state;
}

bool OfflinePageMetadataStoreSQL::CheckDb() const {
  return db_ && state_ == StoreState::LOADED;
}

}  // namespace offline_pages
