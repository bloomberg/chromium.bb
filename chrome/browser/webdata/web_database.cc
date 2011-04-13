// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_database.h"

#include <algorithm>

#include "app/sql/statement.h"
#include "app/sql/transaction.h"
#include "chrome/browser/diagnostics/sqlite_diagnostics.h"
#include "content/common/notification_service.h"

namespace {

// Current version number.  Note: when changing the current version number,
// corresponding changes must happen in the unit tests, and new migration test
// added.  See |WebDatabaseMigrationTest::kCurrentTestedVersionNumber|.
const int kCurrentVersionNumber = 37;
const int kCompatibleVersionNumber = 37;

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
  return autofill_table_.get();
}

KeywordTable* WebDatabase::GetKeywordTable() {
  return keyword_table_.get();
}

LoginsTable* WebDatabase::GetLoginsTable() {
  return logins_table_.get();
}

TokenServiceTable* WebDatabase::GetTokenServiceTable() {
  return token_service_table_.get();
}

WebAppsTable* WebDatabase::GetWebAppsTable() {
  return web_apps_table_.get();
}

sql::Connection* WebDatabase::GetSQLConnection() {
  return &db_;
}

sql::InitStatus WebDatabase::Init(const FilePath& db_name) {
  // When running in unit tests, there is already a NotificationService object.
  // Since only one can exist at a time per thread, check first.
  if (!NotificationService::current())
    notification_service_.reset(new NotificationService);

  // Set the exceptional sqlite error handler.
  db_.set_error_delegate(GetErrorHandlerForWebDb());

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

  // Create the tables.
  autofill_table_.reset(new AutofillTable(&db_, &meta_table_));
  keyword_table_.reset(new KeywordTable(&db_, &meta_table_));
  logins_table_.reset(new LoginsTable(&db_, &meta_table_));
  token_service_table_.reset(new TokenServiceTable(&db_, &meta_table_));
  web_apps_table_.reset(new WebAppsTable(&db_, &meta_table_));

  // Initialize the tables.
  if (!keyword_table_->Init() || !autofill_table_->Init() ||
      !logins_table_->Init() || !web_apps_table_->Init() ||
      !token_service_table_->Init()) {
    LOG(WARNING) << "Unable to initialize the web database.";
    return sql::INIT_FAILURE;
  }

  // If the file on disk is an older database version, bring it up to date.
  // If the migration fails we return an error to caller and do not commit
  // the migration.
  sql::InitStatus migration_status = MigrateOldVersionsAsNeeded();
  if (migration_status != sql::INIT_OK)
    return migration_status;

  return transaction.Commit() ? sql::INIT_OK : sql::INIT_FAILURE;
}

sql::InitStatus WebDatabase::MigrateOldVersionsAsNeeded() {
  // Migrate if necessary.
  int current_version = meta_table_.GetVersionNumber();
  switch (current_version) {
    // Versions 1 - 19 are unhandled.  Version numbers greater than
    // kCurrentVersionNumber should have already been weeded out by the caller.
    default:
      // When the version is too old, we return failure error code.  The schema
      // is too out of date to migrate.
      // There should not be a released product that makes a database too old to
      // migrate. If we do encounter such a legacy database, we will need a
      // better solution to handle it (i.e., pop up a dialog to tell the user,
      // erase all their prefs and start over, etc.).
      LOG(WARNING) << "Web database version " << current_version <<
          " is too old to handle.";
      NOTREACHED();
      return sql::INIT_FAILURE;

    case 20:
      if (!keyword_table_->MigrateToVersion21AutoGenerateKeywordColumn())
        return FailedMigrationTo(21);

      ChangeVersion(&meta_table_, 21, true);
      // FALL THROUGH

    case 21:
      if (!autofill_table_->ClearAutofillEmptyValueElements())
        return FailedMigrationTo(22);

      ChangeVersion(&meta_table_, 22, false);
      // FALL THROUGH

    case 22:
      if (!autofill_table_->MigrateToVersion23AddCardNumberEncryptedColumn())
        return FailedMigrationTo(23);

      ChangeVersion(&meta_table_, 23, false);
      // FALL THROUGH

    case 23:
      if (!autofill_table_->MigrateToVersion24CleanupOversizedStringFields())
        return FailedMigrationTo(24);

      ChangeVersion(&meta_table_, 24, false);
      // FALL THROUGH

    case 24:
      if (!keyword_table_->MigrateToVersion25AddLogoIDColumn())
        return FailedMigrationTo(25);

      ChangeVersion(&meta_table_, 25, true);
      // FALL THROUGH

    case 25:
      if (!keyword_table_->MigrateToVersion26AddCreatedByPolicyColumn())
        return FailedMigrationTo(26);

      ChangeVersion(&meta_table_, 26, true);
      // FALL THROUGH

    case 26:
      if (!autofill_table_->MigrateToVersion27UpdateLegacyCreditCards())
        return FailedMigrationTo(27);

      ChangeVersion(&meta_table_, 27, true);
      // FALL THROUGH

    case 27:
      if (!keyword_table_->MigrateToVersion28SupportsInstantColumn())
        return FailedMigrationTo(28);

      ChangeVersion(&meta_table_, 28, true);
      // FALL THROUGH

    case 28:
      if (!keyword_table_->MigrateToVersion29InstantUrlToSupportsInstant())
        return FailedMigrationTo(29);

      ChangeVersion(&meta_table_, 29, true);
      // FALL THROUGH

    case 29:
      if (!autofill_table_->MigrateToVersion30AddDateModifed())
        return FailedMigrationTo(30);

      ChangeVersion(&meta_table_, 30, true);
      // FALL THROUGH

    case 30:
      if (!autofill_table_->MigrateToVersion31AddGUIDToCreditCardsAndProfiles())
        return FailedMigrationTo(31);

      ChangeVersion(&meta_table_, 31, true);
      // FALL THROUGH

    case 31:
      if (!autofill_table_->MigrateToVersion32UpdateProfilesAndCreditCards())
        return FailedMigrationTo(32);

      ChangeVersion(&meta_table_, 32, true);
      // FALL THROUGH

    case 32:
      if (!autofill_table_->MigrateToVersion33ProfilesBasedOnFirstName())
        return FailedMigrationTo(33);

      ChangeVersion(&meta_table_, 33, true);
      // FALL THROUGH

    case 33:
      if (!autofill_table_->MigrateToVersion34ProfilesBasedOnCountryCode())
        return FailedMigrationTo(34);

      ChangeVersion(&meta_table_, 34, true);
      // FALL THROUGH

    case 34:
      if (!autofill_table_->MigrateToVersion35GreatBritainCountryCodes())
        return FailedMigrationTo(35);

      ChangeVersion(&meta_table_, 35, true);
      // FALL THROUGH

    // Combine migrations 35 and 36.  This is due to enhancements to the merge
    // step when migrating profiles.  The original migration from 35 to 36 did
    // not merge profiles with identical addresses, but the migration from 36 to
    // 37 does.  The step from 35 to 36 should only happen on the Chrome 12 dev
    // channel.  Chrome 12 beta and release users will jump from 35 to 37
    // directly getting the full benefits of the multi-valued merge as well as
    // the culling of bad data.
    case 35:
    case 36:
      if (!autofill_table_->MigrateToVersion37MergeAndCullOlderProfiles())
        return FailedMigrationTo(37);

      ChangeVersion(&meta_table_, 37, true);
      // FALL THROUGH

    // Add successive versions here.  Each should set the version number and
    // compatible version number as appropriate, then fall through to the next
    // case.

    case kCurrentVersionNumber:
      // No migration needed.
      return sql::INIT_OK;
  }
}
