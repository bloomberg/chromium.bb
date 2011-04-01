// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webdata/web_database.h"

#include <algorithm>
#include <limits>
#include <set>
#include <string>

#include "app/sql/statement.h"
#include "app/sql/transaction.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/autofill/autofill_country.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/diagnostics/sqlite_diagnostics.h"
#include "chrome/browser/webdata/autofill_util.h"
#include "chrome/common/guid.h"
#include "content/common/notification_service.h"

using base::Time;

namespace {

// Current version number.  Note: when changing the current version number,
// corresponding changes must happen in the unit tests, and new migration test
// added.  See |WebDatabaseMigrationTest::kCurrentTestedVersionNumber|.
const int kCurrentVersionNumber = 36;
const int kCompatibleVersionNumber = 36;

// TODO(dhollowa): Find a common place for this.  It is duplicated in
// personal_data_manager.cc.
template<typename T>
T* address_of(T& v) {
  return &v;
}

}  // anonymous namespace

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
      // Add the autogenerate_keyword column.
      if (!db_.Execute("ALTER TABLE keywords ADD COLUMN autogenerate_keyword "
                       "INTEGER DEFAULT 0")) {
        LOG(WARNING) << "Unable to update web database to version 21.";
        NOTREACHED();
        return sql::INIT_FAILURE;
      }
      meta_table_.SetVersionNumber(21);
      meta_table_.SetCompatibleVersionNumber(
          std::min(21, kCompatibleVersionNumber));
      // FALL THROUGH

    case 21:
      if (!autofill_table_->ClearAutofillEmptyValueElements()) {
        LOG(WARNING) << "Failed to clean-up autofill DB.";
        NOTREACHED();
        return sql::INIT_FAILURE;
      }
      meta_table_.SetVersionNumber(22);
      // No change in the compatibility version number.

      // FALL THROUGH

    case 22:
      // Add the card_number_encrypted column if credit card table was not
      // created in this build (otherwise the column already exists).
      // WARNING: Do not change the order of the execution of the SQL
      // statements in this case!  Profile corruption and data migration
      // issues WILL OCCUR. (see http://crbug.com/10913)
      //
      // The problem is that if a user has a profile which was created before
      // r37036, when the credit_cards table was added, and then failed to
      // update this profile between the credit card addition and the addition
      // of the "encrypted" columns (44963), the next data migration will put
      // the user's profile in an incoherent state: The user will update from
      // a data profile set to be earlier than 22, and therefore pass through
      // this update case.  But because the user did not have a credit_cards
      // table before starting Chrome, it will have just been initialized
      // above, and so already have these columns -- and thus this data
      // update step will have failed.
      //
      // The false assumption in this case is that at this step in the
      // migration, the user has a credit card table, and that this
      // table does not include encrypted columns!
      // Because this case does not roll back the complete set of SQL
      // transactions properly in case of failure (that is, it does not
      // roll back the table initialization done above), the incoherent
      // profile will now see itself as being at version 22 -- but include a
      // fully initialized credit_cards table.  Every time Chrome runs, it
      // will try to update the web database and fail at this step, unless
      // we allow for the faulty assumption described above by checking for
      // the existence of the columns only AFTER we've executed the commands
      // to add them.
      if (!db_.DoesColumnExist("credit_cards", "card_number_encrypted")) {
        if (!db_.Execute("ALTER TABLE credit_cards ADD COLUMN "
                         "card_number_encrypted BLOB DEFAULT NULL")) {
          LOG(WARNING) << "Could not add card_number_encrypted to "
                          "credit_cards table.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }
      }

      if (!db_.DoesColumnExist("credit_cards", "verification_code_encrypted")) {
        if (!db_.Execute("ALTER TABLE credit_cards ADD COLUMN "
                         "verification_code_encrypted BLOB DEFAULT NULL")) {
          LOG(WARNING) << "Could not add verification_code_encrypted to "
                          "credit_cards table.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }
      }
      meta_table_.SetVersionNumber(23);
      // FALL THROUGH

    case 23: {
      // One-time cleanup for Chromium bug 38364.  In the presence of
      // multi-byte UTF-8 characters, that bug could cause Autofill strings
      // to grow larger and more corrupt with each save.  The cleanup removes
      // any row with a string field larger than a reasonable size.  The string
      // fields examined here are precisely the ones that were subject to
      // corruption by the original bug.
      const std::string autofill_is_too_big =
          "max(length(name), length(value)) > 500";

      const std::string credit_cards_is_too_big =
          "max(length(label), length(name_on_card), length(type), "
          "    length(expiration_month), length(expiration_year), "
          "    length(billing_address), length(shipping_address) "
          ") > 500";

      const std::string autofill_profiles_is_too_big =
          "max(length(label), length(first_name), "
          "    length(middle_name), length(last_name), length(email), "
          "    length(company_name), length(address_line_1), "
          "    length(address_line_2), length(city), length(state), "
          "    length(zipcode), length(country), length(phone), "
          "    length(fax)) > 500";

      std::string query = "DELETE FROM autofill_dates WHERE pair_id IN ("
          "SELECT pair_id FROM autofill WHERE " + autofill_is_too_big + ")";
      if (!db_.Execute(query.c_str())) {
        LOG(WARNING) << "Unable to update web database to version 24.";
        NOTREACHED();
        return sql::INIT_FAILURE;
      }
      query = "DELETE FROM autofill WHERE " + autofill_is_too_big;
      if (!db_.Execute(query.c_str())) {
        LOG(WARNING) << "Unable to update web database to version 24.";
        NOTREACHED();
        return sql::INIT_FAILURE;
      }
      // Only delete from legacy credit card tables where specific columns
      // exist.
      if (db_.DoesColumnExist("credit_cards", "label") &&
          db_.DoesColumnExist("credit_cards", "name_on_card") &&
          db_.DoesColumnExist("credit_cards", "type") &&
          db_.DoesColumnExist("credit_cards", "expiration_month") &&
          db_.DoesColumnExist("credit_cards", "expiration_year") &&
          db_.DoesColumnExist("credit_cards", "billing_address") &&
          db_.DoesColumnExist("credit_cards", "shipping_address") &&
          db_.DoesColumnExist("autofill_profiles", "label")) {
        query = "DELETE FROM credit_cards WHERE (" + credit_cards_is_too_big +
            ") OR label IN (SELECT label FROM autofill_profiles WHERE " +
            autofill_profiles_is_too_big + ")";
        if (!db_.Execute(query.c_str())) {
          LOG(WARNING) << "Unable to update web database to version 24.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }
      }
      if (db_.DoesColumnExist("autofill_profiles", "label")) {
        query = "DELETE FROM autofill_profiles WHERE " +
            autofill_profiles_is_too_big;
        if (!db_.Execute(query.c_str())) {
          LOG(WARNING) << "Unable to update web database to version 24.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }
      }

      meta_table_.SetVersionNumber(24);

      // FALL THROUGH
    }

    case 24:
      // Add the logo_id column if keyword table was not created in this build.
      if (!db_.Execute("ALTER TABLE keywords ADD COLUMN logo_id "
                       "INTEGER DEFAULT 0")) {
        LOG(WARNING) << "Unable to update web database to version 25.";
        NOTREACHED();
        return sql::INIT_FAILURE;
      }
      meta_table_.SetVersionNumber(25);
      meta_table_.SetCompatibleVersionNumber(
          std::min(25, kCompatibleVersionNumber));
      // FALL THROUGH

    case 25:
      // Add the created_by_policy column.
      if (!db_.Execute("ALTER TABLE keywords ADD COLUMN created_by_policy "
                       "INTEGER DEFAULT 0")) {
        LOG(WARNING) << "Unable to update web database to version 26.";
        NOTREACHED();
        return sql::INIT_FAILURE;
      }

      meta_table_.SetVersionNumber(26);
      meta_table_.SetCompatibleVersionNumber(
          std::min(26, kCompatibleVersionNumber));
      // FALL THROUGH

    case 26: {
      // Only migrate from legacy credit card tables where specific columns
      // exist.
      if (db_.DoesColumnExist("credit_cards", "unique_id") &&
          db_.DoesColumnExist("credit_cards", "billing_address") &&
          db_.DoesColumnExist("autofill_profiles", "unique_id")) {
        // Change the credit_cards.billing_address column from a string to an
        // int.  The stored string is the label of an address, so we have to
        // select the unique ID of this address using the label as a foreign
        // key into the |autofill_profiles| table.
        std::string stmt =
            "SELECT credit_cards.unique_id, autofill_profiles.unique_id "
            "FROM autofill_profiles, credit_cards "
            "WHERE credit_cards.billing_address = autofill_profiles.label";
        sql::Statement s(db_.GetUniqueStatement(stmt.c_str()));
        if (!s) {
          LOG(WARNING) << "Statement prepare failed";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        std::map<int, int> cc_billing_map;
        while (s.Step())
          cc_billing_map[s.ColumnInt(0)] = s.ColumnInt(1);

        // Windows already stores the IDs as strings in |billing_address|. Try
        // to convert those.
        if (cc_billing_map.empty()) {
          std::string stmt =
            "SELECT unique_id,billing_address FROM credit_cards";
          sql::Statement s(db_.GetUniqueStatement(stmt.c_str()));
          if (!s) {
            LOG(WARNING) << "Statement prepare failed";
            NOTREACHED();
            return sql::INIT_FAILURE;
          }

          while (s.Step()) {
            int id = 0;
            if (base::StringToInt(s.ColumnString(1), &id))
              cc_billing_map[s.ColumnInt(0)] = id;
          }
        }

        if (!db_.Execute("CREATE TABLE credit_cards_temp ( "
                         "label VARCHAR, "
                         "unique_id INTEGER PRIMARY KEY, "
                         "name_on_card VARCHAR, "
                         "type VARCHAR, "
                         "card_number VARCHAR, "
                         "expiration_month INTEGER, "
                         "expiration_year INTEGER, "
                         "verification_code VARCHAR, "
                         "billing_address INTEGER, "
                         "shipping_address VARCHAR, "
                         "card_number_encrypted BLOB, "
                         "verification_code_encrypted BLOB)")) {
          LOG(WARNING) << "Unable to update web database to version 27.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute(
            "INSERT INTO credit_cards_temp "
            "SELECT label,unique_id,name_on_card,type,card_number,"
            "expiration_month,expiration_year,verification_code,0,"
            "shipping_address,card_number_encrypted,"
            "verification_code_encrypted FROM credit_cards")) {
          LOG(WARNING) << "Unable to update web database to version 27.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute("DROP TABLE credit_cards")) {
          LOG(WARNING) << "Unable to update web database to version 27.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute(
            "ALTER TABLE credit_cards_temp RENAME TO credit_cards")) {
          LOG(WARNING) << "Unable to update web database to version 27.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        for (std::map<int, int>::const_iterator iter = cc_billing_map.begin();
             iter != cc_billing_map.end(); ++iter) {
          sql::Statement s(db_.GetCachedStatement(
              SQL_FROM_HERE,
              "UPDATE credit_cards SET billing_address=? WHERE unique_id=?"));
          if (!s) {
            LOG(WARNING) << "Statement prepare failed";
            NOTREACHED();
            return sql::INIT_FAILURE;
          }

          s.BindInt(0, (*iter).second);
          s.BindInt(1, (*iter).first);

          if (!s.Run()) {
            LOG(WARNING) << "Unable to update web database to version 27.";
            NOTREACHED();
            return sql::INIT_FAILURE;
          }
        }
      }

      meta_table_.SetVersionNumber(27);
      meta_table_.SetCompatibleVersionNumber(
          std::min(27, kCompatibleVersionNumber));

      // FALL THROUGH
    }

    case 27:
      // Add supports_instant to keywords.
      if (!db_.Execute("ALTER TABLE keywords ADD COLUMN supports_instant "
                       "INTEGER DEFAULT 0")) {
        LOG(WARNING) << "Unable to update web database to version 28.";
        NOTREACHED();
        return sql::INIT_FAILURE;
      }
      meta_table_.SetVersionNumber(28);
      meta_table_.SetCompatibleVersionNumber(
          std::min(28, kCompatibleVersionNumber));

      // FALL THROUGH

    case 28:
      // Keywords loses the column supports_instant and gets instant_url.
      if (!db_.Execute("ALTER TABLE keywords ADD COLUMN instant_url "
                       "VARCHAR")) {
        LOG(WARNING) << "Unable to update web database to version 29.";
        NOTREACHED();
        return sql::INIT_FAILURE;
      }
      if (!db_.Execute("CREATE TABLE keywords_temp ("
                       "id INTEGER PRIMARY KEY,"
                       "short_name VARCHAR NOT NULL,"
                       "keyword VARCHAR NOT NULL,"
                       "favicon_url VARCHAR NOT NULL,"
                       "url VARCHAR NOT NULL,"
                       "show_in_default_list INTEGER,"
                       "safe_for_autoreplace INTEGER,"
                       "originating_url VARCHAR,"
                       "date_created INTEGER DEFAULT 0,"
                       "usage_count INTEGER DEFAULT 0,"
                       "input_encodings VARCHAR,"
                       "suggest_url VARCHAR,"
                       "prepopulate_id INTEGER DEFAULT 0,"
                       "autogenerate_keyword INTEGER DEFAULT 0,"
                       "logo_id INTEGER DEFAULT 0,"
                       "created_by_policy INTEGER DEFAULT 0,"
                       "instant_url VARCHAR)")) {
        LOG(WARNING) << "Unable to update web database to version 29.";
        NOTREACHED();
        return sql::INIT_FAILURE;
      }

      if (!db_.Execute(
          "INSERT INTO keywords_temp "
          "SELECT id, short_name, keyword, favicon_url, url, "
          "show_in_default_list, safe_for_autoreplace, originating_url, "
          "date_created, usage_count, input_encodings, suggest_url, "
          "prepopulate_id, autogenerate_keyword, logo_id, created_by_policy, "
          "instant_url FROM keywords")) {
        LOG(WARNING) << "Unable to update web database to version 29.";
        NOTREACHED();
        return sql::INIT_FAILURE;
      }

      if (!db_.Execute("DROP TABLE keywords")) {
        LOG(WARNING) << "Unable to update web database to version 29.";
        NOTREACHED();
        return sql::INIT_FAILURE;
      }

      if (!db_.Execute(
          "ALTER TABLE keywords_temp RENAME TO keywords")) {
        LOG(WARNING) << "Unable to update web database to version 29.";
        NOTREACHED();
        return sql::INIT_FAILURE;
      }

      meta_table_.SetVersionNumber(29);
      meta_table_.SetCompatibleVersionNumber(
          std::min(29, kCompatibleVersionNumber));

      // FALL THROUGH

    case 29:
      // Add date_modified to autofill_profiles.
      if (!db_.DoesColumnExist("autofill_profiles", "date_modified")) {
        if (!db_.Execute("ALTER TABLE autofill_profiles ADD COLUMN "
                         "date_modified INTEGER NON NULL DEFAULT 0")) {
          LOG(WARNING) << "Unable to update web database to version 30";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        sql::Statement s(db_.GetUniqueStatement(
            "UPDATE autofill_profiles SET date_modified=?"));
        if (!s) {
          LOG(WARNING) << "Unable to update web database to version 30.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        s.BindInt64(0, Time::Now().ToTimeT());

        if (!s.Run()) {
          LOG(WARNING) << "Unable to update web database to version 30.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

      }

      // Add date_modified to credit_cards.
      if (!db_.DoesColumnExist("credit_cards", "date_modified")) {
        if (!db_.Execute("ALTER TABLE credit_cards ADD COLUMN "
                         "date_modified INTEGER NON NULL DEFAULT 0")) {
          LOG(WARNING) << "Unable to update web database to version 30";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        sql::Statement s(db_.GetUniqueStatement(
            "UPDATE credit_cards SET date_modified=?"));
        if (!s) {
          LOG(WARNING) << "Unable to update web database to version 30.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        s.BindInt64(0, Time::Now().ToTimeT());

        if (!s.Run()) {
          LOG(WARNING) << "Unable to update web database to version 30.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }
      }

      meta_table_.SetVersionNumber(30);
      meta_table_.SetCompatibleVersionNumber(
          std::min(30, kCompatibleVersionNumber));

      // FALL THROUGH

    case 30:
      // Add |guid| column to |autofill_profiles| table.
      // Note that we need to check for the guid column's existence due to the
      // fact that for a version 22 database the |autofill_profiles| table
      // gets created fresh with |InitAutofillProfilesTable|.
      if (!db_.DoesColumnExist("autofill_profiles", "guid")) {
        if (!db_.Execute("ALTER TABLE autofill_profiles ADD COLUMN "
                         "guid VARCHAR NOT NULL DEFAULT \"\"")) {
          LOG(WARNING) << "Unable to update web database to version 30.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        // Set all the |guid| fields to valid values.
        {
          sql::Statement s(db_.GetUniqueStatement("SELECT unique_id "
                                                  "FROM autofill_profiles"));

          if (!s) {
            LOG(WARNING) << "Unable to update web database to version 30.";
            NOTREACHED();
            return sql::INIT_FAILURE;
          }

          while (s.Step()) {
            sql::Statement update_s(
                db_.GetUniqueStatement("UPDATE autofill_profiles "
                                       "SET guid=? WHERE unique_id=?"));
            if (!update_s) {
              LOG(WARNING) << "Unable to update web database to version 30.";
              NOTREACHED();
              return sql::INIT_FAILURE;
            }
            update_s.BindString(0, guid::GenerateGUID());
            update_s.BindInt(1, s.ColumnInt(0));

            if (!update_s.Run()) {
              LOG(WARNING) << "Unable to update web database to version 30.";
              NOTREACHED();
              return sql::INIT_FAILURE;
            }
          }
        }
      }

      // Add |guid| column to |credit_cards| table.
      // Note that we need to check for the guid column's existence due to the
      // fact that for a version 22 database the |autofill_profiles| table
      // gets created fresh with |InitAutofillProfilesTable|.
      if (!db_.DoesColumnExist("credit_cards", "guid")) {
        if (!db_.Execute("ALTER TABLE credit_cards ADD COLUMN "
                         "guid VARCHAR NOT NULL DEFAULT \"\"")) {
          LOG(WARNING) << "Unable to update web database to version 30.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        // Set all the |guid| fields to valid values.
        {
          sql::Statement s(db_.GetUniqueStatement("SELECT unique_id "
                                                  "FROM credit_cards"));
          if (!s) {
            LOG(WARNING) << "Unable to update web database to version 30.";
            NOTREACHED();
            return sql::INIT_FAILURE;
          }

          while (s.Step()) {
            sql::Statement update_s(
                db_.GetUniqueStatement("UPDATE credit_cards "
                                       "set guid=? WHERE unique_id=?"));
            if (!update_s) {
              LOG(WARNING) << "Unable to update web database to version 30.";
              NOTREACHED();
              return sql::INIT_FAILURE;
            }
            update_s.BindString(0, guid::GenerateGUID());
            update_s.BindInt(1, s.ColumnInt(0));

            if (!update_s.Run()) {
              LOG(WARNING) << "Unable to update web database to version 30.";
              NOTREACHED();
              return sql::INIT_FAILURE;
            }
          }
        }
      }

      meta_table_.SetVersionNumber(31);
      meta_table_.SetCompatibleVersionNumber(
          std::min(31, kCompatibleVersionNumber));

      // FALL THROUGH

    case 31:
      if (db_.DoesColumnExist("autofill_profiles", "unique_id")) {
        if (!db_.Execute("CREATE TABLE autofill_profiles_temp ( "
                         "guid VARCHAR PRIMARY KEY, "
                         "label VARCHAR, "
                         "first_name VARCHAR, "
                         "middle_name VARCHAR, "
                         "last_name VARCHAR, "
                         "email VARCHAR, "
                         "company_name VARCHAR, "
                         "address_line_1 VARCHAR, "
                         "address_line_2 VARCHAR, "
                         "city VARCHAR, "
                         "state VARCHAR, "
                         "zipcode VARCHAR, "
                         "country VARCHAR, "
                         "phone VARCHAR, "
                         "fax VARCHAR, "
                         "date_modified INTEGER NOT NULL DEFAULT 0)")) {
          LOG(WARNING) << "Unable to update web database to version 32.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute(
            "INSERT INTO autofill_profiles_temp "
            "SELECT guid, label, first_name, middle_name, last_name, email, "
            "company_name, address_line_1, address_line_2, city, state, "
            "zipcode, country, phone, fax, date_modified "
            "FROM autofill_profiles")) {
          LOG(WARNING) << "Unable to update web database to version 32.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute("DROP TABLE autofill_profiles")) {
          LOG(WARNING) << "Unable to update web database to version 32.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute(
            "ALTER TABLE autofill_profiles_temp RENAME TO autofill_profiles")) {
          LOG(WARNING) << "Unable to update web database to version 32.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }
      }

      if (db_.DoesColumnExist("credit_cards", "unique_id")) {
        if (!db_.Execute("CREATE TABLE credit_cards_temp ( "
                         "guid VARCHAR PRIMARY KEY, "
                         "label VARCHAR, "
                         "name_on_card VARCHAR, "
                         "expiration_month INTEGER, "
                         "expiration_year INTEGER, "
                         "card_number_encrypted BLOB, "
                         "date_modified INTEGER NOT NULL DEFAULT 0)")) {
          LOG(WARNING) << "Unable to update web database to version 32.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute(
            "INSERT INTO credit_cards_temp "
            "SELECT guid, label, name_on_card, expiration_month, "
            "expiration_year, card_number_encrypted, date_modified "
            "FROM credit_cards")) {
          LOG(WARNING) << "Unable to update web database to version 32.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute("DROP TABLE credit_cards")) {
          LOG(WARNING) << "Unable to update web database to version 32.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute(
            "ALTER TABLE credit_cards_temp RENAME TO credit_cards")) {
          LOG(WARNING) << "Unable to update web database to version 32.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }
      }

      meta_table_.SetVersionNumber(32);
      meta_table_.SetCompatibleVersionNumber(
          std::min(32, kCompatibleVersionNumber));

      // FALL THROUGH

    case 32:
      // Test the existence of the |first_name| column as an indication that
      // we need a migration.  It is possible that the new |autofill_profiles|
      // schema is in place because the table was newly created when migrating
      // from a pre-version-22 database.
      if (db_.DoesColumnExist("autofill_profiles", "first_name")) {
        // Create autofill_profiles_temp table that will receive the data.
        if (!db_.DoesTableExist("autofill_profiles_temp")) {
          if (!db_.Execute("CREATE TABLE autofill_profiles_temp ( "
                           "guid VARCHAR PRIMARY KEY, "
                           "company_name VARCHAR, "
                           "address_line_1 VARCHAR, "
                           "address_line_2 VARCHAR, "
                           "city VARCHAR, "
                           "state VARCHAR, "
                           "zipcode VARCHAR, "
                           "country VARCHAR, "
                           "date_modified INTEGER NOT NULL DEFAULT 0)")) {
            NOTREACHED();
            return sql::INIT_FAILURE;
          }
        }

        {
          sql::Statement s(db_.GetUniqueStatement(
              "SELECT guid, first_name, middle_name, last_name, email, "
              "company_name, address_line_1, address_line_2, city, state, "
              "zipcode, country, phone, fax, date_modified "
              "FROM autofill_profiles"));
          while (s.Step()) {
            AutofillProfile profile;
            profile.set_guid(s.ColumnString(0));
            DCHECK(guid::IsValidGUID(profile.guid()));

            profile.SetInfo(NAME_FIRST, s.ColumnString16(1));
            profile.SetInfo(NAME_MIDDLE, s.ColumnString16(2));
            profile.SetInfo(NAME_LAST, s.ColumnString16(3));
            profile.SetInfo(EMAIL_ADDRESS, s.ColumnString16(4));
            profile.SetInfo(COMPANY_NAME, s.ColumnString16(5));
            profile.SetInfo(ADDRESS_HOME_LINE1, s.ColumnString16(6));
            profile.SetInfo(ADDRESS_HOME_LINE2, s.ColumnString16(7));
            profile.SetInfo(ADDRESS_HOME_CITY, s.ColumnString16(8));
            profile.SetInfo(ADDRESS_HOME_STATE, s.ColumnString16(9));
            profile.SetInfo(ADDRESS_HOME_ZIP, s.ColumnString16(10));
            profile.SetInfo(ADDRESS_HOME_COUNTRY, s.ColumnString16(11));
            profile.SetInfo(PHONE_HOME_WHOLE_NUMBER, s.ColumnString16(12));
            profile.SetInfo(PHONE_FAX_WHOLE_NUMBER, s.ColumnString16(13));
            int64 date_modified = s.ColumnInt64(14);

            sql::Statement s_insert(db_.GetUniqueStatement(
                "INSERT INTO autofill_profiles_temp"
                "(guid, company_name, address_line_1, address_line_2, city,"
                " state, zipcode, country, date_modified)"
                "VALUES (?,?,?,?,?,?,?,?,?)"));
            if (!s) {
              LOG(WARNING) << "Unable to update web database to version 33.";
              NOTREACHED();
              return sql::INIT_FAILURE;
            }
            s_insert.BindString(0, profile.guid());
            s_insert.BindString16(1, profile.GetInfo(COMPANY_NAME));
            s_insert.BindString16(2, profile.GetInfo(ADDRESS_HOME_LINE1));
            s_insert.BindString16(3, profile.GetInfo(ADDRESS_HOME_LINE2));
            s_insert.BindString16(4, profile.GetInfo(ADDRESS_HOME_CITY));
            s_insert.BindString16(5, profile.GetInfo(ADDRESS_HOME_STATE));
            s_insert.BindString16(6, profile.GetInfo(ADDRESS_HOME_ZIP));
            s_insert.BindString16(7, profile.GetInfo(ADDRESS_HOME_COUNTRY));
            s_insert.BindInt64(8, date_modified);

            if (!s_insert.Run()) {
              NOTREACHED();
              return sql::INIT_FAILURE;
            }

            // Add the other bits: names, emails, and phone/fax.
            if (!autofill_util::AddAutofillProfilePieces(profile, &db_)) {
              NOTREACHED();
              return sql::INIT_FAILURE;
            }
          }
        }

        if (!db_.Execute("DROP TABLE autofill_profiles")) {
          LOG(WARNING) << "Unable to update web database to version 33.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute(
            "ALTER TABLE autofill_profiles_temp RENAME TO autofill_profiles")) {
          LOG(WARNING) << "Unable to update web database to version 33.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }
      }

      // Remove the labels column from the credit_cards table.
      if (db_.DoesColumnExist("credit_cards", "label")) {
        if (!db_.Execute("CREATE TABLE credit_cards_temp ( "
                         "guid VARCHAR PRIMARY KEY, "
                         "name_on_card VARCHAR, "
                         "expiration_month INTEGER, "
                         "expiration_year INTEGER, "
                         "card_number_encrypted BLOB, "
                         "date_modified INTEGER NOT NULL DEFAULT 0)")) {
          LOG(WARNING) << "Unable to update web database to version 33.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute(
            "INSERT INTO credit_cards_temp "
            "SELECT guid, name_on_card, expiration_month, "
            "expiration_year, card_number_encrypted, date_modified "
            "FROM credit_cards")) {
          LOG(WARNING) << "Unable to update web database to version 33.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute("DROP TABLE credit_cards")) {
          LOG(WARNING) << "Unable to update web database to version 33.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        if (!db_.Execute(
            "ALTER TABLE credit_cards_temp RENAME TO credit_cards")) {
          LOG(WARNING) << "Unable to update web database to version 33.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }
      }

      meta_table_.SetVersionNumber(33);
      meta_table_.SetCompatibleVersionNumber(
          std::min(33, kCompatibleVersionNumber));

      // FALL THROUGH

    case 33:
      // Test the existence of the |country_code| column as an indication that
      // we need a migration.  It is possible that the new |autofill_profiles|
      // schema is in place because the table was newly created when migrating
      // from a pre-version-22 database.
      if (!db_.DoesColumnExist("autofill_profiles", "country_code")) {
        if (!db_.Execute("ALTER TABLE autofill_profiles ADD COLUMN "
                         "country_code VARCHAR")) {
          LOG(WARNING) << "Unable to update web database to version 33.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        // Set all the |country_code| fields to match existing |country| values.
        {
          sql::Statement s(db_.GetUniqueStatement("SELECT guid, country "
                                                  "FROM autofill_profiles"));

          if (!s) {
            LOG(WARNING) << "Unable to update web database to version 33.";
            NOTREACHED();
            return sql::INIT_FAILURE;
          }

          while (s.Step()) {
            sql::Statement update_s(
                db_.GetUniqueStatement("UPDATE autofill_profiles "
                                       "SET country_code=? WHERE guid=?"));
            if (!update_s) {
              LOG(WARNING) << "Unable to update web database to version 33.";
              NOTREACHED();
              return sql::INIT_FAILURE;
            }
            string16 country = s.ColumnString16(1);
            std::string app_locale = AutofillCountry::ApplicationLocale();
            update_s.BindString(0, AutofillCountry::GetCountryCode(country,
                                                                   app_locale));
            update_s.BindString(1, s.ColumnString(0));

            if (!update_s.Run()) {
              LOG(WARNING) << "Unable to update web database to version 33.";
              NOTREACHED();
              return sql::INIT_FAILURE;
            }
          }
        }
      }

      meta_table_.SetVersionNumber(34);
      meta_table_.SetCompatibleVersionNumber(
          std::min(34, kCompatibleVersionNumber));

      // FALL THROUGH

    case 34:
      // Correct all country codes with value "UK" to be "GB".  This data
      // was mistakenly introduced in build 686.0.  This migration is to clean
      // it up.  See http://crbug.com/74511 for details.
      {
        sql::Statement s(db_.GetUniqueStatement(
            "UPDATE autofill_profiles SET country_code=\"GB\" "
            "WHERE country_code=\"UK\""));

        if (!s.Run()) {
          LOG(WARNING) << "Unable to update web database to version 35.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }
      }

      meta_table_.SetVersionNumber(35);
      meta_table_.SetCompatibleVersionNumber(
          std::min(35, kCompatibleVersionNumber));

      // FALL THROUGH

    case 35:
      // Merge and cull older profiles where possible.
      {
        sql::Statement s(db_.GetUniqueStatement(
            "SELECT guid, date_modified "
            "FROM autofill_profiles"));
        if (!s) {
          NOTREACHED() << "Statement prepare failed";
          return sql::INIT_FAILURE;
        }

        // Accumulate the good profiles.
        std::vector<AutofillProfile> accumulated_profiles;
        std::vector<AutofillProfile*> accumulated_profiles_p;
        std::map<std::string, int64> modification_map;
        while (s.Step()) {
          std::string guid = s.ColumnString(0);
          int64 date_modified = s.ColumnInt64(1);
          modification_map.insert(
              std::pair<std::string, int64>(guid, date_modified));
          AutofillProfile* profile = NULL;
          if (!autofill_table_->GetAutofillProfile(guid, &profile)) {
            NOTREACHED() << "Bad read of profile.";
            return sql::INIT_FAILURE;
          }
          scoped_ptr<AutofillProfile> p(profile);

          if (PersonalDataManager::IsValidLearnableProfile(*p)) {
            std::vector<AutofillProfile> merged_profiles;
            bool merged = PersonalDataManager::MergeProfile(
                *p, accumulated_profiles_p, &merged_profiles);

            std::swap(accumulated_profiles, merged_profiles);

            accumulated_profiles_p.clear();
            accumulated_profiles_p.resize(accumulated_profiles.size());
            std::transform(accumulated_profiles.begin(),
                           accumulated_profiles.end(),
                           accumulated_profiles_p.begin(),
                           address_of<AutofillProfile>);

            // If the profile got merged trash the original.
            if (merged)
              autofill_table_->AddAutofillGUIDToTrash(p->guid());
          } else {
            // An invalid profile, so trash it.
            autofill_table_->AddAutofillGUIDToTrash(p->guid());
          }
        }

        // Drop the current profiles.
        if (!autofill_table_->ClearAutofillProfiles()) {
          LOG(WARNING) << "Unable to update web database to version 36.";
          NOTREACHED();
          return sql::INIT_FAILURE;
        }

        // Add the newly merged profiles back in.
        for (std::vector<AutofillProfile>::const_iterator
                iter = accumulated_profiles.begin();
             iter != accumulated_profiles.end();
             ++iter) {
          if (!autofill_table_->AddAutofillProfile(*iter)) {
            LOG(WARNING) << "Unable to update web database to version 36.";
            NOTREACHED();
            return sql::INIT_FAILURE;
          }

          // Fix up the original modification date.
          std::map<std::string, int64>::const_iterator date_item =
              modification_map.find(iter->guid());
          if (date_item == modification_map.end()) {
            LOG(WARNING) << "Unable to update web database to version 36.";
            NOTREACHED();
            return sql::INIT_FAILURE;
          }
          sql::Statement s_date(db_.GetUniqueStatement(
              "UPDATE autofill_profiles SET date_modified=? "
              "WHERE guid=?"));
          s_date.BindInt64(0, date_item->second);
          s_date.BindString(1, iter->guid());
          if (!s_date.Run()) {
            LOG(WARNING) << "Unable to update web database to version 36.";
            NOTREACHED();
            return sql::INIT_FAILURE;
          }
        }
      }

      meta_table_.SetVersionNumber(36);
      meta_table_.SetCompatibleVersionNumber(
          std::min(36, kCompatibleVersionNumber));

      // FALL THROUGH

    // Add successive versions here.  Each should set the version number and
    // compatible version number as appropriate, then fall through to the next
    // case.

    case kCurrentVersionNumber:
      // No migration needed.
      return sql::INIT_OK;
  }
}
