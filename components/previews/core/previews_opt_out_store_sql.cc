// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/previews_opt_out_store_sql.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/previews/core/previews_black_list.h"
#include "components/previews/core/previews_black_list_item.h"
#include "components/previews/core/previews_experiments.h"
#include "sql/connection.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace previews {

namespace {

// This is a macro instead of a const, so it can be used inline in other SQL
// statements below.
#define PREVIEWS_TABLE_NAME "previews_v1"

void CreateSchema(sql::Connection* db) {
  const char kSql[] = "CREATE TABLE IF NOT EXISTS " PREVIEWS_TABLE_NAME
                      " (host_name VARCHAR NOT NULL,"
                      " time INTEGER NOT NULL,"
                      " opt_out INTEGER NOT NULL,"
                      " type INTEGER NOT NULL,"
                      " PRIMARY KEY(host_name, time DESC, opt_out, type))";
  if (!db->Execute(kSql))
    return;
}

void DatabaseErrorCallback(sql::Connection* db,
                           const base::FilePath& db_path,
                           int extended_error,
                           sql::Statement* stmt) {
  if (sql::Recovery::ShouldRecover(extended_error)) {
    // Prevent reentrant calls.
    db->reset_error_callback();

    // After this call, the |db| handle is poisoned so that future calls will
    // return errors until the handle is re-opened.
    sql::Recovery::RecoverDatabase(db, db_path);

    // The DLOG(FATAL) below is intended to draw immediate attention to errors
    // in newly-written code.  Database corruption is generally a result of OS
    // or hardware issues, not coding errors at the client level, so displaying
    // the error would probably lead to confusion.  The ignored call signals the
    // test-expectation framework that the error was handled.
    ignore_result(sql::Connection::IsExpectedSqliteError(extended_error));
    return;
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Connection::IsExpectedSqliteError(extended_error))
    DLOG(FATAL) << db->GetErrorMessage();
}

void InitDatabase(sql::Connection* db, base::FilePath path) {
  // The entry size should be between 11 and 10 + x bytes, where x is the the
  // length of the host name string in bytes.
  // The total number of entries per host is bounded at 32, and the total number
  // of hosts is currently unbounded (but typically expected to be under 100).
  // Assuming average of 100 bytes per entry, and 100 hosts, the total size will
  // be 4096 * 78. 250 allows room for extreme cases such as many host names
  // or very long host names.
  // The average case should be much smaller as users rarely visit hosts that
  // are not in their top 20 hosts. It should be closer to 32 * 100 * 20 for
  // most users, which is about 4096 * 15.
  // The total size of the database will be capped at 3200 entries.
  db->set_page_size(4096);
  db->set_cache_size(250);
  db->set_histogram_tag("PreviewsOptOut");
  db->set_exclusive_locking();

  db->set_error_callback(base::Bind(&DatabaseErrorCallback, db, path));

  base::File::Error err;
  if (!base::CreateDirectoryAndGetError(path.DirName(), &err)) {
    return;
  }
  if (!db->Open(path)) {
    return;
  }

  CreateSchema(db);
}

void LoadBlackListFromDataBase(
    sql::Connection* db,
    scoped_refptr<base::SingleThreadTaskRunner> runner,
    LoadBlackListCallback callback) {
  // Gets the table sorted by host and time. Limits the number of hosts using
  // most recent opt_out time as the limiting function.
  const char kSql[] =
      "SELECT host_name, time, opt_out"
      " FROM " PREVIEWS_TABLE_NAME;

  sql::Statement statement(db->GetUniqueStatement(kSql));

  std::unique_ptr<BlackListItemMap> black_list_item_map(new BlackListItemMap());
  // Add the host name, the visit time, and opt out history to
  // |black_list_item_map|.
  while (statement.Step()) {
    std::string host_name = statement.ColumnString(0);
    PreviewsBlackListItem* black_list_item =
        PreviewsBlackList::GetOrCreateBlackListItem(black_list_item_map.get(),
                                                    host_name);
    DCHECK_LE(black_list_item_map->size(),
              params::MaxInMemoryHostsInBlackList());
    // Allows the internal logic of PreviewsBlackListItem to determine how to
    // evict entries when there are more than
    // |StoredHistoryLengthForBlackList()| for the host.
    black_list_item->AddPreviewNavigation(
        statement.ColumnBool(2),
        base::Time::FromInternalValue(statement.ColumnInt64(1)));
  }

  runner->PostTask(FROM_HERE,
                   base::Bind(callback, base::Passed(&black_list_item_map)));
}

// Synchronous implementation, this is run on the background thread
// and actually does the work to access SQL.
void LoadBlackListSync(sql::Connection* db,
                       const base::FilePath& path,
                       scoped_refptr<base::SingleThreadTaskRunner> runner,
                       LoadBlackListCallback callback) {
  if (!db->is_open())
    InitDatabase(db, path);

  LoadBlackListFromDataBase(db, runner, callback);
}

}  // namespace

PreviewsOptOutStoreSQL::PreviewsOptOutStoreSQL(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const base::FilePath& path)
    : io_task_runner_(io_task_runner),
      background_task_runner_(background_task_runner),
      db_file_path_(path) {}

PreviewsOptOutStoreSQL::~PreviewsOptOutStoreSQL() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (db_.get()) {
    background_task_runner_->DeleteSoon(FROM_HERE, db_.release());
  }
}

void PreviewsOptOutStoreSQL::AddPreviewNavigation(bool opt_out,
                                                  const std::string& host_name,
                                                  PreviewsType type,
                                                  base::Time now) {}

void PreviewsOptOutStoreSQL::ClearBlackList(base::Time begin_time,
                                            base::Time end_time) {}

void PreviewsOptOutStoreSQL::LoadBlackList(LoadBlackListCallback callback) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!db_)
    db_ = base::MakeUnique<sql::Connection>();
  background_task_runner_->PostTask(
      FROM_HERE, base::Bind(&LoadBlackListSync, db_.get(), db_file_path_,
                            base::ThreadTaskRunnerHandle::Get(), callback));
}

}  // namespace previews
