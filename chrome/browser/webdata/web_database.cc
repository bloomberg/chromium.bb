// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_database.h"

#include <algorithm>

#include "chrome/browser/webdata/autofill_table.h"
#include "chrome/browser/webdata/keyword_table.h"
#include "chrome/browser/webdata/logins_table.h"
#include "chrome/browser/webdata/token_service_table.h"
#include "chrome/browser/webdata/web_apps_table.h"
#include "chrome/browser/webdata/web_intents_table.h"
#include "content/public/browser/notification_service.h"
#include "sql/statement.h"
#include "sql/transaction.h"

// Current version number.  Note: when changing the current version number,
// corresponding changes must happen in the unit tests, and new migration test
// added.  See |WebDatabaseMigrationTest::kCurrentTestedVersionNumber|.
// static
const int WebDatabase::kCurrentVersionNumber = 49;

namespace {

const int kCompatibleVersionNumber = 48;

// Change the version number and possibly the compatibility version of
// |meta_table_|.
void ChangeVersion(sql::MetaTable* meta_table,
                   int version_num,
                   bool update_compatible_version_num) {
  meta_table->SetVersionNumber(version_num);
  if (update_compatible_version_num) {
    meta_table->SetCompatibleVersionNumber(
          std::min(version_num, kCompatibleVersionNumber));
  }
}

// Outputs the failed version number as a warning and always returns
// |sql::INIT_FAILURE|.
sql::InitStatus FailedMigrationTo(int version_num) {
  LOG(WARNING) << "Unable to update web database to version "
               << version_num << ".";
  NOTREACHED();
  return sql::INIT_FAILURE;
}

}  // namespace

WebDatabase::WebDatabase() {}

WebDatabase::~WebDatabase() {}

void WebDatabase::BeginTransaction() {
  db_.BeginTransaction();
}

void WebDatabase::CommitTransaction() {
  db_.CommitTransaction();
}

AutofillTable* WebDatabase::GetAutofillTable() {
  return autofill_table_;
}

KeywordTable* WebDatabase::GetKeywordTable() {
  return keyword_table_;
}

LoginsTable* WebDatabase::GetLoginsTable() {
  return logins_table_;
}

TokenServiceTable* WebDatabase::GetTokenServiceTable() {
  return token_service_table_;
}

WebAppsTable* WebDatabase::GetWebAppsTable() {
  return web_apps_table_;
}

sql::Connection* WebDatabase::GetSQLConnection() {
  return &db_;
}

sql::InitStatus WebDatabase::Init(const base::FilePath& db_name,
                                  const std::string& app_locale) {
  // When running in unit tests, there is already a NotificationService object.
  // Since only one can exist at a time per thread, check first.
  if (!content::NotificationService::current())
    notification_service_.reset(content::NotificationService::Create());

  db_.set_error_histogram_name("Sqlite.Web.Error");

  // We don't store that much data in the tables so use a small page size.
  // This provides a large benefit for empty tables (which is very likely with
  // the tables we create).
  db_.set_page_size(2048);

  // We shouldn't have much data and what access we currently have is quite
  // infrequent. So we go with a small cache size.
  db_.set_cache_size(32);

  // Run the database in exclusive mode. Nobody else should be accessing the
  // database while we're running, and this will give somewhat improved perf.
  db_.set_exclusive_locking();

  if (!db_.Open(db_name))
    return sql::INIT_FAILURE;

  // Initialize various tables
  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return sql::INIT_FAILURE;

  // Version check.
  if (!meta_table_.Init(&db_, kCurrentVersionNumber, kCompatibleVersionNumber))
    return sql::INIT_FAILURE;
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Web database is too new.";
    return sql::INIT_TOO_NEW;
  }

  // TODO(joi): Table creation should move out of this class; switch
  // to a two-phase init to accomplish this.

  // Create the tables.
  autofill_table_ = new AutofillTable(&db_, &meta_table_);
  tables_.push_back(autofill_table_);

  keyword_table_ = new KeywordTable(&db_, &meta_table_);
  tables_.push_back(keyword_table_);

  // TODO(mdm): We only really need the LoginsTable on Windows for IE7 password
  // access, but for now, we still create it on all platforms since it deletes
  // the old logins table. We can remove this after a while, e.g. in M22 or so.
  logins_table_ = new LoginsTable(&db_, &meta_table_);
  tables_.push_back(logins_table_);

  token_service_table_ = new TokenServiceTable(&db_, &meta_table_);
  tables_.push_back(token_service_table_);

  web_apps_table_ = new WebAppsTable(&db_, &meta_table_);
  tables_.push_back(web_apps_table_);

  web_intents_table_ = new WebIntentsTable(&db_, &meta_table_);
  tables_.push_back(web_intents_table_);

  // Initialize the tables.
  for (ScopedVector<WebDatabaseTable>::iterator it = tables_.begin();
       it != tables_.end();
       ++it) {
    if (!(*it)->Init()) {
      LOG(WARNING) << "Unable to initialize the web database.";
      return sql::INIT_FAILURE;
    }
  }

  // If the file on disk is an older database version, bring it up to date.
  // If the migration fails we return an error to caller and do not commit
  // the migration.
  sql::InitStatus migration_status = MigrateOldVersionsAsNeeded(app_locale);
  if (migration_status != sql::INIT_OK)
    return migration_status;

  return transaction.Commit() ? sql::INIT_OK : sql::INIT_FAILURE;
}

sql::InitStatus WebDatabase::MigrateOldVersionsAsNeeded(
    const std::string& app_locale) {
  // Some malware used to lower the version number, causing migration to
  // fail. Ensure the version number is at least as high as the compatible
  // version number.
  int current_version = std::max(meta_table_.GetVersionNumber(),
                                 meta_table_.GetCompatibleVersionNumber());
  if (current_version > meta_table_.GetVersionNumber())
    ChangeVersion(&meta_table_, current_version, false);

  if (current_version < 20) {
    // Versions 1 - 19 are unhandled.  Version numbers greater than
    // kCurrentVersionNumber should have already been weeded out by the caller.
    //
    // When the version is too old, we return failure error code.  The schema
    // is too out of date to migrate.
    //
    // There should not be a released product that makes a database too old to
    // migrate. If we do encounter such a legacy database, we will need a
    // better solution to handle it (i.e., pop up a dialog to tell the user,
    // erase all their prefs and start over, etc.).
    LOG(WARNING) << "Web database version " << current_version <<
        " is too old to handle.";
    NOTREACHED();
    return sql::INIT_FAILURE;
  }

  for (int next_version = current_version + 1;
       next_version <= kCurrentVersionNumber;
       ++next_version) {
    // Give each table a chance to migrate to this version.
    for (ScopedVector<WebDatabaseTable>::iterator it = tables_.begin();
         it != tables_.end();
         ++it) {
      // Any of the tables may set this to true, but by default it is false.
      bool update_compatible_version = false;
      if (!(*it)->MigrateToVersion(next_version,
                                   app_locale,
                                   &update_compatible_version)) {
        return FailedMigrationTo(next_version);
      }

      ChangeVersion(&meta_table_, next_version, update_compatible_version);
    }
  }
  return sql::INIT_OK;
}
