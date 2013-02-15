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

void ActivityDatabase::Init(const base::FilePath& db_name) {
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

  // Create the DOMAction database.
  if (!DOMAction::InitializeTable(&db_))
    return LogInitFailure();

  // Create the APIAction database.
  if (!APIAction::InitializeTable(&db_))
    return LogInitFailure();

  // Create the BlockedAction database.
  if (!BlockedAction::InitializeTable(&db_))
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
  db_.RazeAndClose();
}

}  // namespace extensions
