// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "app/sql/transaction.h"
#include "base/string_util.h"
#include "chrome/browser/history/archived_database.h"

namespace history {

namespace {

static const int kCurrentVersionNumber = 3;
static const int kCompatibleVersionNumber = 2;

}  // namespace

ArchivedDatabase::ArchivedDatabase() {
}

ArchivedDatabase::~ArchivedDatabase() {
}

bool ArchivedDatabase::Init(const FilePath& file_name) {
  // Set the database page size to something a little larger to give us
  // better performance (we're typically seek rather than bandwidth limited).
  // This only has an effect before any tables have been created, otherwise
  // this is a NOP. Must be a power of 2 and a max of 8192.
  db_.set_page_size(4096);

  // Don't use very much memory caching this database. We seldom use it for
  // anything important.
  db_.set_cache_size(64);

  // Run the database in exclusive mode. Nobody else should be accessing the
  // database while we're running, and this will give somewhat improved perf.
  db_.set_exclusive_locking();

  if (!db_.Open(file_name))
    return false;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    db_.Close();
    return false;
  }

  // Version check.
  if (!meta_table_.Init(&db_, kCurrentVersionNumber,
                        kCompatibleVersionNumber)) {
    db_.Close();
    return false;
  }

  // Create the tables.
  if (!CreateURLTable(false) || !InitVisitTable() ||
      !InitKeywordSearchTermsTable()) {
    db_.Close();
    return false;
  }
  CreateMainURLIndex();
  CreateKeywordSearchTermsIndices();

  if (EnsureCurrentVersion() != sql::INIT_OK) {
    db_.Close();
    return false;
  }

  return transaction.Commit();
}

void ArchivedDatabase::BeginTransaction() {
  db_.BeginTransaction();
}

void ArchivedDatabase::CommitTransaction() {
  db_.CommitTransaction();
}

sql::Connection& ArchivedDatabase::GetDB() {
  return db_;
}

// Migration -------------------------------------------------------------------

sql::InitStatus ArchivedDatabase::EnsureCurrentVersion() {
  // We can't read databases newer than we were designed for.
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Archived database is too new.";
    return sql::INIT_TOO_NEW;
  }

  // NOTICE: If you are changing structures for things shared with the archived
  // history file like URLs, visits, or downloads, that will need migration as
  // well. Instead of putting such migration code in this class, it should be
  // in the corresponding file (url_database.cc, etc.) and called from here and
  // from the archived_database.cc.

  int cur_version = meta_table_.GetVersionNumber();
  if (cur_version == 1) {
    if (!DropStarredIDFromURLs()) {
      LOG(WARNING) << "Unable to update archived database to version 2.";
      return sql::INIT_FAILURE;
    }
    ++cur_version;
    meta_table_.SetVersionNumber(cur_version);
    meta_table_.SetCompatibleVersionNumber(
        std::min(cur_version, kCompatibleVersionNumber));
  }

  if (cur_version == 2) {
    // This is the version prior to adding visit_source table.
    ++cur_version;
    meta_table_.SetVersionNumber(cur_version);
  }

  // Put future migration cases here.

  // When the version is too old, we just try to continue anyway, there should
  // not be a released product that makes a database too old for us to handle.
  LOG_IF(WARNING, cur_version < kCurrentVersionNumber) <<
      "Archived database version " << cur_version << " is too old to handle.";

  return sql::INIT_OK;
}
}  // namespace history
