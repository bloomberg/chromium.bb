// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/store/prefetch_store.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/offline_store_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_utils.h"
#include "sql/connection.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {
namespace {

// Name of the table with prefetch items.
const char kPrefetchItemsTableName[] = "prefetch_items";
const char kPrefetchStoreFileName[] = "PrefetchStore.db";

const int kCurrentVersion = 1;
const int kCompatibleVersion = 1;

using InitializeCallback =
    base::Callback<void(InitializationStatus,
                        std::unique_ptr<sql::Connection>)>;

// IMPORTANT: when making changes to these columns please also reflect them
// into:
// - PrefetchItem: update existing fields and all method implementations
//   (operator=, operator<<, ToString, etc).
// - PrefetchItemTest, PrefetchStoreTestUtil: update test related code to cover
//   the changed set of columns and PrefetchItem members.
// - MockPrefetchItemGenerator: so that its generated items consider all fields.
// IMPORTANT #2: the order of columns types is also important in SQLite tables
// for optimizing space utilization. Fixed length types must come first and
// variable length types later.
static const char kTableCreationSql[] =
    "CREATE TABLE prefetch_items"
    // Fixed length columns come first.
    "(offline_id INTEGER PRIMARY KEY NOT NULL,"
    " state INTEGER NOT NULL DEFAULT 0,"
    " generate_bundle_attempts INTEGER NOT NULL DEFAULT 0,"
    " get_operation_attempts INTEGER NOT NULL DEFAULT 0,"
    " download_initiation_attempts INTEGER NOT NULL DEFAULT 0,"
    " archive_body_length INTEGER_NOT_NULL DEFAULT -1,"
    " creation_time INTEGER NOT NULL,"
    " freshness_time INTEGER NOT NULL,"
    " error_code INTEGER NOT NULL DEFAULT 0,"
    " file_size INTEGER NOT NULL DEFAULT 0,"
    // Variable length columns come later.
    " guid VARCHAR NOT NULL DEFAULT '',"
    " client_namespace VARCHAR NOT NULL DEFAULT '',"
    " client_id VARCHAR NOT NULL DEFAULT '',"
    " requested_url VARCHAR NOT NULL DEFAULT '',"
    " final_archived_url VARCHAR NOT NULL DEFAULT '',"
    " operation_name VARCHAR NOT NULL DEFAULT '',"
    " archive_body_name VARCHAR NOT NULL DEFAULT '',"
    " title VARCHAR NOT NULL DEFAULT '',"
    " file_path VARCHAR NOT NULL DEFAULT ''"
    ")";

bool CreatePrefetchItemsTable(sql::Connection* db) {
  return db->Execute(kTableCreationSql);
}

bool CreateSchema(sql::Connection* db) {
  // If you create a transaction but don't Commit() it is automatically
  // rolled back by its destructor when it falls out of scope.
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  if (!db->DoesTableExist(kPrefetchItemsTableName)) {
    if (!CreatePrefetchItemsTable(db))
      return false;
  }

  sql::MetaTable meta_table;
  meta_table.Init(db, kCurrentVersion, kCompatibleVersion);

  // This would be a great place to add indices when we need them.
  return transaction.Commit();
}

bool PrepareDirectory(const base::FilePath& path) {
  base::File::Error error = base::File::FILE_OK;
  if (!base::DirectoryExists(path.DirName())) {
    if (!base::CreateDirectoryAndGetError(path.DirName(), &error)) {
      LOG(ERROR) << "Failed to create prefetch db directory: "
                 << base::File::ErrorToString(error);
      return false;
    }
  }
  return true;
}

// TODO(fgorski): This function and this part of the system in general could
// benefit from a better status code reportable through UMA to better capture
// the reason for failure, aiding the process of repeated attempts to
// open/initialize the database.
bool InitializeSync(sql::Connection* db,
                    const base::FilePath& path,
                    bool in_memory) {
  // These values are default.
  db->set_page_size(4096);
  db->set_cache_size(500);
  db->set_histogram_tag("PrefetchStore");
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

}  // namespace

PrefetchStore::PrefetchStore(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner)
    : blocking_task_runner_(std::move(blocking_task_runner)),
      in_memory_(true),
      db_(new sql::Connection,
          base::OnTaskRunnerDeleter(blocking_task_runner_)),
      initialization_status_(InitializationStatus::NOT_INITIALIZED),
      weak_ptr_factory_(this) {}

PrefetchStore::PrefetchStore(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const base::FilePath& path)
    : blocking_task_runner_(std::move(blocking_task_runner)),
      db_file_path_(path.AppendASCII(kPrefetchStoreFileName)),
      in_memory_(false),
      db_(new sql::Connection,
          base::OnTaskRunnerDeleter(blocking_task_runner_)),
      initialization_status_(InitializationStatus::NOT_INITIALIZED),
      weak_ptr_factory_(this) {}

PrefetchStore::~PrefetchStore() {}

void PrefetchStore::Initialize(base::OnceClosure pending_command) {
  DCHECK_EQ(initialization_status_, InitializationStatus::NOT_INITIALIZED);

  initialization_status_ = InitializationStatus::INITIALIZING;
  base::PostTaskAndReplyWithResult(
      blocking_task_runner_.get(), FROM_HERE,
      base::BindOnce(&InitializeSync, db_.get(), db_file_path_, in_memory_),
      base::BindOnce(&PrefetchStore::OnInitializeDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(pending_command)));
}

void PrefetchStore::OnInitializeDone(base::OnceClosure pending_command,
                                     bool success) {
  DCHECK_EQ(initialization_status_, InitializationStatus::INITIALIZING);
  initialization_status_ =
      success ? InitializationStatus::SUCCESS : InitializationStatus::FAILURE;

  CHECK(!pending_command.is_null());
  std::move(pending_command).Run();

  // Once pending commands are empty, we get back to NOT_INITIALIZED state, to
  // make it possible to retry initialization next time a DB operation is
  // attempted.
  if (initialization_status_ == InitializationStatus::FAILURE)
    initialization_status_ = InitializationStatus::NOT_INITIALIZED;
}

// static
const char* PrefetchStore::GetTableCreationSqlForTesting() {
  return kTableCreationSql;
}

}  // namespace offline_pages
