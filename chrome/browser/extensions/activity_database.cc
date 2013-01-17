// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/extensions/activity_database.h"
#include "chrome/browser/history/url_database.h"
#include "content/public/browser/browser_thread.h"
#include "sql/transaction.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

using content::BrowserThread;

namespace extensions {

ActivityDatabase::ActivityDatabase() : initialized_(false) {}

ActivityDatabase::~ActivityDatabase() {
  Close();  // Safe to call Close() even if Open() never happened.
}

void ActivityDatabase::SetErrorDelegate(sql::ErrorDelegate* error_delegate) {
  db_.set_error_delegate(error_delegate);
}

void ActivityDatabase::Init(const FilePath& db_name) {
  db_.set_page_size(4096);
  db_.set_cache_size(32);

  if (!db_.Open(db_name)) {
    LOG(ERROR) << db_.GetErrorMessage();
    return LogInitFailure();
  }

  // Wrap the initialization in a transaction so that the db doesn't
  // get corrupted if init fails/crashes.
  sql::Transaction committer(&db_);
  if (!committer.Begin())
    return LogInitFailure();

#if defined(OS_MACOSX)
  // Exclude the database from backups.
  base::mac::SetFileBackupExclusion(db_name);
#endif

  db_.Preload();

  // Create the UrlAction database.
  if (InitializeTable(UrlAction::kTableName, UrlAction::kTableStructure) !=
      sql::INIT_OK)
    return LogInitFailure();

  // Create the APIAction database.
  if (InitializeTable(APIAction::kTableName, APIAction::kTableStructure)
      != sql::INIT_OK)
    return LogInitFailure();

  // Create the BlockedAction database.
  if (InitializeTable(BlockedAction::kTableName, BlockedAction::kTableStructure)
      != sql::INIT_OK)
    return LogInitFailure();

  sql::InitStatus stat = committer.Commit() ? sql::INIT_OK : sql::INIT_FAILURE;
  if (stat == sql::INIT_OK)
    initialized_ = true;
  else
    return LogInitFailure();
}

void ActivityDatabase::LogInitFailure() {
  LOG(ERROR) << "Couldn't initialize the activity log database.";
}

sql::InitStatus ActivityDatabase::InitializeTable(const char* table_name,
                                                  const char* table_structure) {
  if (!db_.DoesTableExist(table_name)) {
    char table_creator[1000];
    base::snprintf(table_creator,
                   arraysize(table_creator),
                   "CREATE TABLE %s %s", table_name, table_structure);
    if (!db_.Execute(table_creator))
      return sql::INIT_FAILURE;
  }
  return sql::INIT_OK;
}

void ActivityDatabase::RecordAction(scoped_refptr<Action> action) {
  if (initialized_)
    action->Record(&db_);
}

void ActivityDatabase::BeginTransaction() {
  db_.BeginTransaction();
}

void ActivityDatabase::CommitTransaction() {
  db_.CommitTransaction();
}

void ActivityDatabase::RollbackTransaction() {
  db_.RollbackTransaction();
}

bool ActivityDatabase::Raze() {
  return db_.Raze();
}

void ActivityDatabase::Close() {
  db_.Close();
}

void ActivityDatabase::KillDatabase() {
  db_.RollbackTransaction();
  db_.Raze();
  db_.Close();
}

}  // namespace extensions

