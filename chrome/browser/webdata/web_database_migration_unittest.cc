// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/stl_util.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/webdata/autofill_change.h"
#include "chrome/browser/webdata/autofill_entry.h"
#include "chrome/browser/webdata/web_database.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/guid.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/test/test_browser_thread.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using content::BrowserThread;

namespace {

void AutofillProfile31FromStatement(const sql::Statement& s,
                                    AutofillProfile* profile,
                                    string16* label,
                                    int* unique_id,
                                    int64* date_modified) {
  DCHECK(profile);
  DCHECK(label);
  DCHECK(unique_id);
  DCHECK(date_modified);
  *label = s.ColumnString16(0);
  *unique_id = s.ColumnInt(1);
  profile->SetInfo(NAME_FIRST, s.ColumnString16(2));
  profile->SetInfo(NAME_MIDDLE, s.ColumnString16(3));
  profile->SetInfo(NAME_LAST, s.ColumnString16(4));
  profile->SetInfo(EMAIL_ADDRESS, s.ColumnString16(5));
  profile->SetInfo(COMPANY_NAME, s.ColumnString16(6));
  profile->SetInfo(ADDRESS_HOME_LINE1, s.ColumnString16(7));
  profile->SetInfo(ADDRESS_HOME_LINE2, s.ColumnString16(8));
  profile->SetInfo(ADDRESS_HOME_CITY, s.ColumnString16(9));
  profile->SetInfo(ADDRESS_HOME_STATE, s.ColumnString16(10));
  profile->SetInfo(ADDRESS_HOME_ZIP, s.ColumnString16(11));
  profile->SetInfo(ADDRESS_HOME_COUNTRY, s.ColumnString16(12));
  profile->SetInfo(PHONE_HOME_WHOLE_NUMBER, s.ColumnString16(13));
  *date_modified = s.ColumnInt64(15);
  profile->set_guid(s.ColumnString(16));
  EXPECT_TRUE(guid::IsValidGUID(profile->guid()));
}

void AutofillProfile33FromStatement(const sql::Statement& s,
                                    AutofillProfile* profile,
                                    int64* date_modified) {
  DCHECK(profile);
  DCHECK(date_modified);
  profile->set_guid(s.ColumnString(0));
  EXPECT_TRUE(guid::IsValidGUID(profile->guid()));
  profile->SetInfo(COMPANY_NAME, s.ColumnString16(1));
  profile->SetInfo(ADDRESS_HOME_LINE1, s.ColumnString16(2));
  profile->SetInfo(ADDRESS_HOME_LINE2, s.ColumnString16(3));
  profile->SetInfo(ADDRESS_HOME_CITY, s.ColumnString16(4));
  profile->SetInfo(ADDRESS_HOME_STATE, s.ColumnString16(5));
  profile->SetInfo(ADDRESS_HOME_ZIP, s.ColumnString16(6));
  profile->SetInfo(ADDRESS_HOME_COUNTRY, s.ColumnString16(7));
  *date_modified = s.ColumnInt64(8);
}

void CreditCard31FromStatement(const sql::Statement& s,
                              CreditCard* credit_card,
                              string16* label,
                              int* unique_id,
                              std::string* encrypted_number,
                              int64* date_modified) {
  DCHECK(credit_card);
  DCHECK(label);
  DCHECK(unique_id);
  DCHECK(encrypted_number);
  DCHECK(date_modified);
  *label = s.ColumnString16(0);
  *unique_id = s.ColumnInt(1);
  credit_card->SetInfo(CREDIT_CARD_NAME, s.ColumnString16(2));
  credit_card->SetInfo(CREDIT_CARD_TYPE, s.ColumnString16(3));
  credit_card->SetInfo(CREDIT_CARD_EXP_MONTH, s.ColumnString16(5));
  credit_card->SetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, s.ColumnString16(6));
  int encrypted_number_len = s.ColumnByteLength(10);
  if (encrypted_number_len) {
    encrypted_number->resize(encrypted_number_len);
    memcpy(&(*encrypted_number)[0], s.ColumnBlob(10), encrypted_number_len);
  }
  *date_modified = s.ColumnInt64(12);
  credit_card->set_guid(s.ColumnString(13));
  EXPECT_TRUE(guid::IsValidGUID(credit_card->guid()));
}

void CreditCard32FromStatement(const sql::Statement& s,
                               CreditCard* credit_card,
                               std::string* encrypted_number,
                               int64* date_modified) {
  DCHECK(credit_card);
  DCHECK(encrypted_number);
  DCHECK(date_modified);
  credit_card->set_guid(s.ColumnString(0));
  EXPECT_TRUE(guid::IsValidGUID(credit_card->guid()));
  credit_card->SetInfo(CREDIT_CARD_NAME, s.ColumnString16(1));
  credit_card->SetInfo(CREDIT_CARD_EXP_MONTH, s.ColumnString16(2));
  credit_card->SetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, s.ColumnString16(3));
  int encrypted_number_len = s.ColumnByteLength(4);
  if (encrypted_number_len) {
    encrypted_number->resize(encrypted_number_len);
    memcpy(&(*encrypted_number)[0], s.ColumnBlob(4), encrypted_number_len);
  }
  *date_modified = s.ColumnInt64(5);
}

}  // anonymous namespace

// The WebDatabaseMigrationTest encapsulates testing of database migrations.
// Specifically, these tests are intended to exercise any schema changes in
// the WebDatabase and data migrations that occur in
// |WebDatabase::MigrateOldVersionsAsNeeded()|.
class WebDatabaseMigrationTest : public testing::Test {
 public:
  // In order to access the application locale -- which the tested functions do
  // internally -- this test must run on the UI thread.
  // TODO(isherman): The WebDatabase code should probably verify that it is
  // running on the DB thread.  Once that verification is added, this code will
  // need to be updated to create both threads.
  WebDatabaseMigrationTest()
      : ui_thread_(BrowserThread::UI, &message_loop_for_ui_) {}
  virtual ~WebDatabaseMigrationTest() {}

  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

 protected:
  // Current tested version number.  When adding a migration in
  // |WebDatabase::MigrateOldVersionsAsNeeded()| and changing the version number
  // |kCurrentVersionNumber| this value should change to reflect the new version
  // number and a new migration test added below.
  static const int kCurrentTestedVersionNumber;

  FilePath GetDatabasePath() {
    const FilePath::CharType kWebDatabaseFilename[] =
        FILE_PATH_LITERAL("TestWebDatabase.sqlite3");
    return temp_dir_.path().Append(FilePath(kWebDatabaseFilename));
  }

  // The textual contents of |file| are read from
  // "chrome/test/data/web_database" and returned in the string |contents|.
  // Returns true if the file exists and is read successfully, false otherwise.
  bool GetWebDatabaseData(const FilePath& file, std::string* contents) {
    FilePath path = ui_test_utils::GetTestFilePath(
        FilePath(FILE_PATH_LITERAL("web_database")), file);
    return file_util::PathExists(path) &&
        file_util::ReadFileToString(path, contents);
  }

  static int VersionFromConnection(sql::Connection* connection) {
    // Get version.
    sql::Statement s(connection->GetUniqueStatement(
        "SELECT value FROM meta WHERE key='version'"));
    if (!s.Step())
      return 0;
    return s.ColumnInt(0);
  }

  // The sql files located in "chrome/test/data/web_database" were generated by
  // launching the Chromium application prior to schema change, then using the
  // sqlite3 command-line application to dump the contents of the "Web Data"
  // database.
  // Like this:
  //   > .output version_nn.sql
  //   > .dump
  void LoadDatabase(const FilePath::StringType& file);

  // Assertion testing for migrating from version 27 and 28.
  void MigrateVersion28Assertions();

 private:
  MessageLoopForUI message_loop_for_ui_;
  content::TestBrowserThread ui_thread_;
  ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(WebDatabaseMigrationTest);
};

const int WebDatabaseMigrationTest::kCurrentTestedVersionNumber = 41;

void WebDatabaseMigrationTest::LoadDatabase(const FilePath::StringType& file) {
  std::string contents;
  ASSERT_TRUE(GetWebDatabaseData(FilePath(file), &contents));

  sql::Connection connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  ASSERT_TRUE(connection.Execute(contents.data()));
}

void WebDatabaseMigrationTest::MigrateVersion28Assertions() {
  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // Make sure supports_instant was dropped and instant_url was added.
    EXPECT_FALSE(connection.DoesColumnExist("keywords", "supports_instant"));
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "instant_url"));

    // Check that instant_url is empty.
    std::string stmt = "SELECT instant_url FROM keywords";
    sql::Statement s(connection.GetUniqueStatement(stmt.c_str()));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ(std::string(), s.ColumnString(0));

    // Verify the data made it over.
    stmt = "SELECT id, short_name, keyword, favicon_url, url, "
        "show_in_default_list, safe_for_autoreplace, originating_url, "
        "date_created, usage_count, input_encodings, suggest_url, "
        "prepopulate_id, autogenerate_keyword, logo_id, created_by_policy, "
        "instant_url FROM keywords";
    sql::Statement s2(connection.GetUniqueStatement(stmt.c_str()));
    ASSERT_TRUE(s2.Step());
    EXPECT_EQ(2, s2.ColumnInt(0));
    EXPECT_EQ("Google", s2.ColumnString(1));
    EXPECT_EQ("google.com", s2.ColumnString(2));
    EXPECT_EQ("http://www.google.com/favicon.ico", s2.ColumnString(3));
    EXPECT_EQ("{google:baseURL}search?{google:RLZ}{google:acceptedSuggestion}"\
        "{google:originalQueryForSuggestion}sourceid=chrome&ie={inputEncoding}"\
        "&q={searchTerms}",
        s2.ColumnString(4));
    EXPECT_EQ(1, s2.ColumnInt(5));
    EXPECT_EQ(1, s2.ColumnInt(6));
    EXPECT_EQ(std::string(), s2.ColumnString(7));
    EXPECT_EQ(0, s2.ColumnInt(8));
    EXPECT_EQ(0, s2.ColumnInt(9));
    EXPECT_EQ(std::string("UTF-8"), s2.ColumnString(10));
    EXPECT_EQ(std::string("{google:baseSuggestURL}search?client=chrome&hl="
                          "{language}&q={searchTerms}"), s2.ColumnString(11));
    EXPECT_EQ(1, s2.ColumnInt(12));
    EXPECT_EQ(1, s2.ColumnInt(13));
    EXPECT_EQ(6245, s2.ColumnInt(14));
    EXPECT_EQ(0, s2.ColumnInt(15));
    EXPECT_EQ(0, s2.ColumnInt(16));
    EXPECT_EQ(std::string(), s2.ColumnString(17));
  }
}

// Tests that the all migrations from an empty database succeed.
TEST_F(WebDatabaseMigrationTest, MigrateEmptyToCurrent) {
  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // Check that expected tables are present.
    EXPECT_TRUE(connection.DoesTableExist("meta"));
    EXPECT_TRUE(connection.DoesTableExist("keywords"));
    EXPECT_TRUE(connection.DoesTableExist("logins"));
    EXPECT_TRUE(connection.DoesTableExist("web_app_icons"));
    EXPECT_TRUE(connection.DoesTableExist("web_apps"));
    EXPECT_TRUE(connection.DoesTableExist("autofill"));
    EXPECT_TRUE(connection.DoesTableExist("autofill_dates"));
    EXPECT_TRUE(connection.DoesTableExist("autofill_profiles"));
    EXPECT_TRUE(connection.DoesTableExist("credit_cards"));
    EXPECT_TRUE(connection.DoesTableExist("token_service"));
  }
}

// Tests that the |credit_card| table gets added to the schema for a version 22
// database.
TEST_F(WebDatabaseMigrationTest, MigrateVersion22ToCurrent) {
  // This schema is taken from a build prior to the addition of the
  // |credit_card| table.  Version 22 of the schema.  Contrast this with the
  // corrupt version below.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_22.sql")));

  // Verify pre-conditions.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // No |credit_card| table prior to version 23.
    ASSERT_FALSE(connection.DoesColumnExist("credit_cards", "guid"));
    ASSERT_FALSE(
        connection.DoesColumnExist("credit_cards", "card_number_encrypted"));
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // |credit_card| table now exists.
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "guid"));
    EXPECT_TRUE(
        connection.DoesColumnExist("credit_cards", "card_number_encrypted"));
  }
}

// Tests that the |credit_card| table gets added to the schema for a corrupt
// version 22 database.  The corruption is that the |credit_cards| table exists
// but the schema version number was not set correctly to 23 or later.  This
// test exercises code introduced to fix bug http://crbug.com/50699 that
// resulted from the corruption.
TEST_F(WebDatabaseMigrationTest, MigrateVersion22CorruptedToCurrent) {
  // This schema is taken from a build after the addition of the |credit_card|
  // table.  Due to a bug in the migration logic the version is set incorrectly
  // to 22 (it should have been updated to 23 at least).
  ASSERT_NO_FATAL_FAILURE(
      LoadDatabase(FILE_PATH_LITERAL("version_22_corrupt.sql")));

  // Verify pre-conditions.  These are expectations for corrupt version 22 of
  // the database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Columns existing and not existing before current version.
    ASSERT_TRUE(connection.DoesColumnExist("credit_cards", "unique_id"));
    ASSERT_TRUE(
        connection.DoesColumnExist("credit_cards", "card_number_encrypted"));
    ASSERT_TRUE(connection.DoesColumnExist("keywords", "id"));
    ASSERT_FALSE(connection.DoesColumnExist("keywords", "logo_id"));
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));


    // Columns existing and not existing before version 25.
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards", "unique_id"));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "guid"));
    EXPECT_TRUE(
        connection.DoesColumnExist("credit_cards", "card_number_encrypted"));
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "id"));
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "logo_id"));
  }
}

// Tests that the |keywords| |logo_id| column gets added to the schema for a
// version 24 database.
TEST_F(WebDatabaseMigrationTest, MigrateVersion24ToCurrent) {
  // This schema is taken from a build prior to the addition of the |keywords|
  // |logo_id| column.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_24.sql")));

  // Verify pre-conditions.  These are expectations for version 24 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Columns existing and not existing before current version.
    ASSERT_TRUE(connection.DoesColumnExist("keywords", "id"));
    ASSERT_FALSE(connection.DoesColumnExist("keywords", "logo_id"));
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // |keywords| |logo_id| column should have been added.
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "id"));
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "logo_id"));
  }
}

// Tests that the |keywords| |created_by_policy| column gets added to the schema
// for a version 25 database.
TEST_F(WebDatabaseMigrationTest, MigrateVersion25ToCurrent) {
  // This schema is taken from a build prior to the addition of the |keywords|
  // |created_by_policy| column.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_25.sql")));

  // Verify pre-conditions.  These are expectations for version 25 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // |keywords| |created_by_policy| column should have been added.
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "id"));
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "created_by_policy"));
  }
}

// Tests that the credit_cards.billing_address column is changed from a string
// to an int whilst preserving the associated billing address. This version of
// the test makes sure a stored label is converted to an ID.
TEST_F(WebDatabaseMigrationTest, MigrateVersion26ToCurrentStringLabels) {
  // This schema is taken from a build prior to the change of column type for
  // credit_cards.billing_address from string to int.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_26.sql")));

  // Verify pre-conditions. These are expectations for version 26 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Columns existing and not existing before current version.
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "billing_address"));

    std::string stmt = "INSERT INTO autofill_profiles"
      "(label, unique_id, first_name, middle_name, last_name, email,"
      " company_name, address_line_1, address_line_2, city, state, zipcode,"
      " country, phone, fax)"
      "VALUES ('Home',1,'','','','','','','','','','','','','')";
    sql::Statement s(connection.GetUniqueStatement(stmt.c_str()));
    ASSERT_TRUE(s.Run());

    // Insert a CC linked to an existing address.
    std::string stmt2 = "INSERT INTO credit_cards"
      "(label, unique_id, name_on_card, type, card_number,"
      " expiration_month, expiration_year, verification_code, billing_address,"
      " shipping_address, card_number_encrypted, verification_code_encrypted)"
      "VALUES ('label',2,'Jack','Visa','1234',2,2012,'','Home','','','')";
    sql::Statement s2(connection.GetUniqueStatement(stmt2.c_str()));
    ASSERT_TRUE(s2.Run());

    // |billing_address| is a string.
    std::string stmt3 = "SELECT billing_address FROM credit_cards";
    sql::Statement s3(connection.GetUniqueStatement(stmt3.c_str()));
    ASSERT_TRUE(s3.Step());
    EXPECT_EQ(s3.ColumnType(0), sql::COLUMN_TYPE_TEXT);
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards", "billing_address"));

    // Verify the credit card data is converted.
    sql::Statement s(connection.GetUniqueStatement(
        "SELECT guid, name_on_card, expiration_month, expiration_year, "
        "card_number_encrypted, date_modified "
        "FROM credit_cards"));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ("Jack", s.ColumnString(1));
    EXPECT_EQ(2, s.ColumnInt(2));
    EXPECT_EQ(2012, s.ColumnInt(3));
    // Column 5 is encrypted number blob.
    // Column 6 is date_modified.
  }
}

// Tests that the credit_cards.billing_address column is changed from a string
// to an int whilst preserving the associated billing address. This version of
// the test makes sure a stored string ID is converted to an integer ID.
TEST_F(WebDatabaseMigrationTest, MigrateVersion26ToCurrentStringIDs) {
  // This schema is taken from a build prior to the change of column type for
  // credit_cards.billing_address from string to int.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_26.sql")));

  // Verify pre-conditions. These are expectations for version 26 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "billing_address"));

    std::string stmt = "INSERT INTO autofill_profiles"
      "(label, unique_id, first_name, middle_name, last_name, email,"
      " company_name, address_line_1, address_line_2, city, state, zipcode,"
      " country, phone, fax)"
      "VALUES ('Home',1,'','','','','','','','','','','','','')";
    sql::Statement s(connection.GetUniqueStatement(stmt.c_str()));
    ASSERT_TRUE(s.Run());

    // Insert a CC linked to an existing address.
    std::string stmt2 = "INSERT INTO credit_cards"
      "(label, unique_id, name_on_card, type, card_number,"
      " expiration_month, expiration_year, verification_code, billing_address,"
      " shipping_address, card_number_encrypted, verification_code_encrypted)"
      "VALUES ('label',2,'Jack','Visa','1234',2,2012,'','1','','','')";
    sql::Statement s2(connection.GetUniqueStatement(stmt2.c_str()));
    ASSERT_TRUE(s2.Run());

    // |billing_address| is a string.
    std::string stmt3 = "SELECT billing_address FROM credit_cards";
    sql::Statement s3(connection.GetUniqueStatement(stmt3.c_str()));
    ASSERT_TRUE(s3.Step());
    EXPECT_EQ(s3.ColumnType(0), sql::COLUMN_TYPE_TEXT);
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // |keywords| |created_by_policy| column should have been added.
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "id"));
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "created_by_policy"));
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards", "billing_address"));

    // Verify the credit card data is converted.
    sql::Statement s(connection.GetUniqueStatement(
        "SELECT guid, name_on_card, expiration_month, expiration_year, "
        "card_number_encrypted, date_modified "
        "FROM credit_cards"));
    ASSERT_TRUE(s.Step());
    EXPECT_EQ("Jack", s.ColumnString(1));
    EXPECT_EQ(2, s.ColumnInt(2));
    EXPECT_EQ(2012, s.ColumnInt(3));
    // Column 5 is encrypted credit card number blo b.
    // Column 6 is date_modified.
  }
}

// Tests migration from 27->current. This test is now the same as 28->current
// as the column added in 28 was nuked in 29.
TEST_F(WebDatabaseMigrationTest, MigrateVersion27ToCurrent) {
  // Initialize the database.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_27.sql")));

  // Verify pre-conditions. These are expectations for version 28 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    ASSERT_FALSE(connection.DoesColumnExist("keywords", "supports_instant"));
    ASSERT_FALSE(connection.DoesColumnExist("keywords", "instant_url"));
  }

  MigrateVersion28Assertions();
}

// Makes sure instant_url is added correctly to keywords.
TEST_F(WebDatabaseMigrationTest, MigrateVersion28ToCurrent) {
  // Initialize the database.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_28.sql")));

  // Verify pre-conditions. These are expectations for version 28 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    ASSERT_TRUE(connection.DoesColumnExist("keywords", "supports_instant"));
    ASSERT_FALSE(connection.DoesColumnExist("keywords", "instant_url"));
  }

  MigrateVersion28Assertions();
}

// Makes sure date_modified is added correctly to autofill_profiles and
// credit_cards.
TEST_F(WebDatabaseMigrationTest, MigrateVersion29ToCurrent) {
  // Initialize the database.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_29.sql")));

  // Verify pre-conditions.  These are expectations for version 29 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles",
                                            "date_modified"));
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards",
                                            "date_modified"));
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  Time pre_creation_time = Time::Now();
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }
  Time post_creation_time = Time::Now();

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // Check that the columns were created.
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles",
                                           "date_modified"));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards",
                                           "date_modified"));

    sql::Statement s_profiles(connection.GetUniqueStatement(
        "SELECT date_modified FROM autofill_profiles "));
    ASSERT_TRUE(s_profiles);
    while (s_profiles.Step()) {
      EXPECT_GE(s_profiles.ColumnInt64(0),
                pre_creation_time.ToTimeT());
      EXPECT_LE(s_profiles.ColumnInt64(0),
                post_creation_time.ToTimeT());
    }
    EXPECT_TRUE(s_profiles.Succeeded());

    sql::Statement s_credit_cards(connection.GetUniqueStatement(
        "SELECT date_modified FROM credit_cards "));
    ASSERT_TRUE(s_credit_cards);
    while (s_credit_cards.Step()) {
      EXPECT_GE(s_credit_cards.ColumnInt64(0),
                pre_creation_time.ToTimeT());
      EXPECT_LE(s_credit_cards.ColumnInt64(0),
                post_creation_time.ToTimeT());
    }
    EXPECT_TRUE(s_credit_cards.Succeeded());
  }
}

// Makes sure guids are added to autofill_profiles and credit_cards tables.
TEST_F(WebDatabaseMigrationTest, MigrateVersion30ToCurrent) {
  // Initialize the database.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_30.sql")));

  // Verify pre-conditions. These are expectations for version 29 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles", "guid"));
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards", "guid"));
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    ASSERT_TRUE(connection.DoesColumnExist("autofill_profiles", "guid"));
    ASSERT_TRUE(connection.DoesColumnExist("credit_cards", "guid"));

    // Check that guids are non-null, non-empty, conforms to guid format, and
    // are different.
    sql::Statement s(
        connection.GetUniqueStatement("SELECT guid FROM autofill_profiles"));

    ASSERT_TRUE(s.Step());
    std::string guid1 = s.ColumnString(0);
    EXPECT_TRUE(guid::IsValidGUID(guid1));

    ASSERT_TRUE(s.Step());
    std::string guid2 = s.ColumnString(0);
    EXPECT_TRUE(guid::IsValidGUID(guid2));

    EXPECT_NE(guid1, guid2);
  }
}

// Removes unique IDs and make GUIDs the primary key.  Also removes unused
// columns.
TEST_F(WebDatabaseMigrationTest, MigrateVersion31ToCurrent) {
  // Initialize the database.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_31.sql")));

  // Verify pre-conditions. These are expectations for version 30 of the
  // database.
  AutofillProfile profile;
  string16 profile_label;
  int profile_unique_id = 0;
  int64 profile_date_modified = 0;
  CreditCard credit_card;
  string16 cc_label;
  int cc_unique_id = 0;
  std::string cc_number_encrypted;
  int64 cc_date_modified = 0;
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Verify existence of columns we'll be changing.
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "guid"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "unique_id"));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "guid"));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "unique_id"));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "type"));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "card_number"));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards",
                                           "verification_code"));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "billing_address"));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "shipping_address"));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards",
                                           "verification_code_encrypted"));

    // Fetch data in the database prior to migration.
    sql::Statement s1(
        connection.GetUniqueStatement(
            "SELECT label, unique_id, first_name, middle_name, last_name, "
            "email, company_name, address_line_1, address_line_2, city, state, "
            "zipcode, country, phone, fax, date_modified, guid "
            "FROM autofill_profiles"));
    ASSERT_TRUE(s1.Step());
    EXPECT_NO_FATAL_FAILURE(AutofillProfile31FromStatement(
        s1, &profile, &profile_label, &profile_unique_id,
        &profile_date_modified));

    sql::Statement s2(
        connection.GetUniqueStatement(
            "SELECT label, unique_id, name_on_card, type, card_number, "
            "expiration_month, expiration_year, verification_code, "
            "billing_address, shipping_address, card_number_encrypted, "
            "verification_code_encrypted, date_modified, guid "
            "FROM credit_cards"));
    ASSERT_TRUE(s2.Step());
    EXPECT_NO_FATAL_FAILURE(CreditCard31FromStatement(s2,
                                                      &credit_card,
                                                      &cc_label,
                                                      &cc_unique_id,
                                                      &cc_number_encrypted,
                                                      &cc_date_modified));

    EXPECT_NE(profile_unique_id, cc_unique_id);
    EXPECT_NE(profile.guid(), credit_card.guid());
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // Verify existence of columns we'll be changing.
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "guid"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles", "unique_id"));
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "guid"));
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards", "unique_id"));
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards", "type"));
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards", "card_number"));
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards",
                                            "verification_code"));
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards", "billing_address"));
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards",
                                            "shipping_address"));
    EXPECT_FALSE(connection.DoesColumnExist("credit_cards",
                                            "verification_code_encrypted"));

    // Verify data in the database after the migration.
    sql::Statement s1(
        connection.GetUniqueStatement(
            "SELECT guid, company_name, address_line_1, address_line_2, "
            "city, state, zipcode, country, date_modified "
            "FROM autofill_profiles"));
    ASSERT_TRUE(s1.Step());

    AutofillProfile profile_a;
    int64 profile_date_modified_a = 0;
    EXPECT_NO_FATAL_FAILURE(AutofillProfile33FromStatement(
        s1, &profile_a, &profile_date_modified_a));
    EXPECT_EQ(profile.guid(), profile_a.guid());
    EXPECT_EQ(profile.GetInfo(COMPANY_NAME),
              profile_a.GetInfo(COMPANY_NAME));
    EXPECT_EQ(profile.GetInfo(ADDRESS_HOME_LINE1),
              profile_a.GetInfo(ADDRESS_HOME_LINE1));
    EXPECT_EQ(profile.GetInfo(ADDRESS_HOME_LINE2),
              profile_a.GetInfo(ADDRESS_HOME_LINE2));
    EXPECT_EQ(profile.GetInfo(ADDRESS_HOME_CITY),
              profile_a.GetInfo(ADDRESS_HOME_CITY));
    EXPECT_EQ(profile.GetInfo(ADDRESS_HOME_STATE),
              profile_a.GetInfo(ADDRESS_HOME_STATE));
    EXPECT_EQ(profile.GetInfo(ADDRESS_HOME_ZIP),
              profile_a.GetInfo(ADDRESS_HOME_ZIP));
    EXPECT_EQ(profile.GetInfo(ADDRESS_HOME_COUNTRY),
              profile_a.GetInfo(ADDRESS_HOME_COUNTRY));
    EXPECT_EQ(profile_date_modified, profile_date_modified_a);

    sql::Statement s2(
        connection.GetUniqueStatement(
            "SELECT guid, name_on_card, expiration_month, "
            "expiration_year, card_number_encrypted, date_modified "
            "FROM credit_cards"));
    ASSERT_TRUE(s2.Step());

    CreditCard credit_card_a;
    string16 cc_label_a;
    std::string cc_number_encrypted_a;
    int64 cc_date_modified_a = 0;
    EXPECT_NO_FATAL_FAILURE(CreditCard32FromStatement(s2,
                                                      &credit_card_a,
                                                      &cc_number_encrypted_a,
                                                      &cc_date_modified_a));
    EXPECT_EQ(credit_card, credit_card_a);
    EXPECT_EQ(cc_label, cc_label_a);
    EXPECT_EQ(cc_number_encrypted, cc_number_encrypted_a);
    EXPECT_EQ(cc_date_modified, cc_date_modified_a);
  }
}

// Factor |autofill_profiles| address information separately from name, email,
// and phone.
TEST_F(WebDatabaseMigrationTest, MigrateVersion32ToCurrent) {
  // Initialize the database.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_32.sql")));

  // Verify pre-conditions. These are expectations for version 32 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Verify existence of columns we'll be changing.
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "guid"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "label"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "first_name"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "middle_name"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "last_name"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "email"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles",
                                           "company_name"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles",
                                           "address_line_1"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles",
                                           "address_line_2"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "city"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "state"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "zipcode"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "country"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "phone"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "fax"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles",
                                           "date_modified"));

    EXPECT_FALSE(connection.DoesTableExist("autofill_profile_names"));
    EXPECT_FALSE(connection.DoesTableExist("autofill_profile_emails"));
    EXPECT_FALSE(connection.DoesTableExist("autofill_profile_phones"));

    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "label"));
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // Verify changes to columns.
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "guid"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles", "label"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles", "first_name"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles",
                                            "middle_name"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles", "last_name"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles", "email"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles",
                                           "company_name"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles",
                                           "address_line_1"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles",
                                           "address_line_2"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "city"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "state"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "zipcode"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles", "country"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles", "phone"));
    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles", "fax"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles",
                                           "date_modified"));

    // New "names" table.
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profile_names", "guid"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profile_names",
                                           "first_name"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profile_names",
                                           "middle_name"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profile_names",
                                           "last_name"));

    // New "emails" table.
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profile_emails", "guid"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profile_emails", "email"));

    // New "phones" table.
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profile_phones", "guid"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profile_phones", "type"));
    EXPECT_TRUE(connection.DoesColumnExist("autofill_profile_phones",
                                           "number"));

    EXPECT_FALSE(connection.DoesColumnExist("credit_cards", "label"));

    // Verify data in the database after the migration.
    sql::Statement s1(
        connection.GetUniqueStatement(
            "SELECT guid, company_name, address_line_1, address_line_2, "
            "city, state, zipcode, country, date_modified "
            "FROM autofill_profiles"));

    // John Doe.
    ASSERT_TRUE(s1.Step());
    EXPECT_EQ("00580526-FF81-EE2A-0546-1AC593A32E2F", s1.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("Doe Enterprises"), s1.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("1 Main St"), s1.ColumnString16(2));
    EXPECT_EQ(ASCIIToUTF16("Apt 1"), s1.ColumnString16(3));
    EXPECT_EQ(ASCIIToUTF16("Los Altos"), s1.ColumnString16(4));
    EXPECT_EQ(ASCIIToUTF16("CA"), s1.ColumnString16(5));
    EXPECT_EQ(ASCIIToUTF16("94022"), s1.ColumnString16(6));
    EXPECT_EQ(ASCIIToUTF16("United States"), s1.ColumnString16(7));
    EXPECT_EQ(1297882100L, s1.ColumnInt64(8));

    // John P. Doe.
    // Gets merged during migration from 35 to 37 due to multi-valued fields.

    // Dave Smith.
    ASSERT_TRUE(s1.Step());
    EXPECT_EQ("4C74A9D8-7EEE-423E-F9C2-E7FA70ED1396", s1.ColumnString(0));
    EXPECT_EQ(string16(), s1.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("2 Main Street"), s1.ColumnString16(2));
    EXPECT_EQ(string16(), s1.ColumnString16(3));
    EXPECT_EQ(ASCIIToUTF16("Los Altos"), s1.ColumnString16(4));
    EXPECT_EQ(ASCIIToUTF16("CA"), s1.ColumnString16(5));
    EXPECT_EQ(ASCIIToUTF16("94022"), s1.ColumnString16(6));
    EXPECT_EQ(ASCIIToUTF16("United States"), s1.ColumnString16(7));
    EXPECT_EQ(1297882100L, s1.ColumnInt64(8));

    // Dave Smith (Part 2).
    ASSERT_TRUE(s1.Step());
    EXPECT_EQ("722DF5C4-F74A-294A-46F0-31FFDED0D635", s1.ColumnString(0));
    EXPECT_EQ(string16(), s1.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("2 Main St"), s1.ColumnString16(2));
    EXPECT_EQ(string16(), s1.ColumnString16(3));
    EXPECT_EQ(ASCIIToUTF16("Los Altos"), s1.ColumnString16(4));
    EXPECT_EQ(ASCIIToUTF16("CA"), s1.ColumnString16(5));
    EXPECT_EQ(ASCIIToUTF16("94022"), s1.ColumnString16(6));
    EXPECT_EQ(ASCIIToUTF16("United States"), s1.ColumnString16(7));
    EXPECT_EQ(1297882100L, s1.ColumnInt64(8));

    // Alfred E Newman.
    // Gets culled during migration from 35 to 36 due to incomplete address.

    // 3 Main St.
    ASSERT_TRUE(s1.Step());
    EXPECT_EQ("9E5FE298-62C7-83DF-6293-381BC589183F", s1.ColumnString(0));
    EXPECT_EQ(string16(), s1.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("3 Main St"), s1.ColumnString16(2));
    EXPECT_EQ(string16(), s1.ColumnString16(3));
    EXPECT_EQ(ASCIIToUTF16("Los Altos"), s1.ColumnString16(4));
    EXPECT_EQ(ASCIIToUTF16("CA"), s1.ColumnString16(5));
    EXPECT_EQ(ASCIIToUTF16("94022"), s1.ColumnString16(6));
    EXPECT_EQ(ASCIIToUTF16("United States"), s1.ColumnString16(7));
    EXPECT_EQ(1297882100L, s1.ColumnInt64(8));

    // That should be all.
    EXPECT_FALSE(s1.Step());

    sql::Statement s2(
        connection.GetUniqueStatement(
            "SELECT guid, first_name, middle_name, last_name "
            "FROM autofill_profile_names"));

    // John Doe.
    ASSERT_TRUE(s2.Step());
    EXPECT_EQ("00580526-FF81-EE2A-0546-1AC593A32E2F", s2.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("John"), s2.ColumnString16(1));
    EXPECT_EQ(string16(), s2.ColumnString16(2));
    EXPECT_EQ(ASCIIToUTF16("Doe"), s2.ColumnString16(3));

    // John P. Doe.  Note same guid as above due to merging of multi-valued
    // fields.
    ASSERT_TRUE(s2.Step());
    EXPECT_EQ("00580526-FF81-EE2A-0546-1AC593A32E2F", s2.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("John"), s2.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("P."), s2.ColumnString16(2));
    EXPECT_EQ(ASCIIToUTF16("Doe"), s2.ColumnString16(3));

    // Dave Smith.
    ASSERT_TRUE(s2.Step());
    EXPECT_EQ("4C74A9D8-7EEE-423E-F9C2-E7FA70ED1396", s2.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("Dave"), s2.ColumnString16(1));
    EXPECT_EQ(string16(), s2.ColumnString16(2));
    EXPECT_EQ(ASCIIToUTF16("Smith"), s2.ColumnString16(3));

    // Dave Smith (Part 2).
    ASSERT_TRUE(s2.Step());
    EXPECT_EQ("722DF5C4-F74A-294A-46F0-31FFDED0D635", s2.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("Dave"), s2.ColumnString16(1));
    EXPECT_EQ(string16(), s2.ColumnString16(2));
    EXPECT_EQ(ASCIIToUTF16("Smith"), s2.ColumnString16(3));

    // Alfred E Newman.
    // Gets culled during migration from 35 to 36 due to incomplete address.

    // 3 Main St.
    ASSERT_TRUE(s2.Step());
    EXPECT_EQ("9E5FE298-62C7-83DF-6293-381BC589183F", s2.ColumnString(0));
    EXPECT_EQ(string16(), s2.ColumnString16(1));
    EXPECT_EQ(string16(), s2.ColumnString16(2));
    EXPECT_EQ(string16(), s2.ColumnString16(3));

    // Should be all.
    EXPECT_FALSE(s2.Step());

    sql::Statement s3(
        connection.GetUniqueStatement(
            "SELECT guid, email "
            "FROM autofill_profile_emails"));

    // John Doe.
    ASSERT_TRUE(s3.Step());
    EXPECT_EQ("00580526-FF81-EE2A-0546-1AC593A32E2F", s3.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("john@doe.com"), s3.ColumnString16(1));

    // John P. Doe.
    // Gets culled during migration from 35 to 37 due to merging of John Doe and
    // John P. Doe addresses.

    // 2 Main Street.
    ASSERT_TRUE(s3.Step());
    EXPECT_EQ("4C74A9D8-7EEE-423E-F9C2-E7FA70ED1396", s3.ColumnString(0));
    EXPECT_EQ(string16(), s3.ColumnString16(1));

    // 2 Main St.
    ASSERT_TRUE(s3.Step());
    EXPECT_EQ("722DF5C4-F74A-294A-46F0-31FFDED0D635", s3.ColumnString(0));
    EXPECT_EQ(string16(), s3.ColumnString16(1));

    // Alfred E Newman.
    // Gets culled during migration from 35 to 36 due to incomplete address.

    // 3 Main St.
    ASSERT_TRUE(s3.Step());
    EXPECT_EQ("9E5FE298-62C7-83DF-6293-381BC589183F", s3.ColumnString(0));
    EXPECT_EQ(string16(), s3.ColumnString16(1));

    // Should be all.
    EXPECT_FALSE(s3.Step());

    sql::Statement s4(
        connection.GetUniqueStatement(
            "SELECT guid, type, number "
            "FROM autofill_profile_phones"));

    // John Doe phone.
    ASSERT_TRUE(s4.Step());
    EXPECT_EQ("00580526-FF81-EE2A-0546-1AC593A32E2F", s4.ColumnString(0));
    EXPECT_EQ(0, s4.ColumnInt(1));  // 0 means phone.
    EXPECT_EQ(ASCIIToUTF16("4151112222"), s4.ColumnString16(2));

    // John Doe fax.
    // Gets culled after fax type removed.

    // John P. Doe phone.
    // Gets culled during migration from 35 to 37 due to merging of John Doe and
    // John P. Doe addresses.

    // John P. Doe fax.
    // Gets culled during migration from 35 to 37 due to merging of John Doe and
    // John P. Doe addresses.

    // 2 Main Street phone.
    ASSERT_TRUE(s4.Step());
    EXPECT_EQ("4C74A9D8-7EEE-423E-F9C2-E7FA70ED1396", s4.ColumnString(0));
    EXPECT_EQ(0, s4.ColumnInt(1));  // 0 means phone.
    EXPECT_EQ(string16(), s4.ColumnString16(2));

    // 2 Main Street fax.
    // Gets culled after fax type removed.

    // 2 Main St phone.
    ASSERT_TRUE(s4.Step());
    EXPECT_EQ("722DF5C4-F74A-294A-46F0-31FFDED0D635", s4.ColumnString(0));
    EXPECT_EQ(0, s4.ColumnInt(1));  // 0 means phone.
    EXPECT_EQ(string16(), s4.ColumnString16(2));

    // 2 Main St fax.
    // Gets culled after fax type removed.

    // Note no phone or fax for Alfred E Newman.

    // 3 Main St phone.
    ASSERT_TRUE(s4.Step());
    EXPECT_EQ("9E5FE298-62C7-83DF-6293-381BC589183F", s4.ColumnString(0));
    EXPECT_EQ(0, s4.ColumnInt(1));  // 0 means phone.
    EXPECT_EQ(string16(), s4.ColumnString16(2));

    // 2 Main St fax.
    // Gets culled after fax type removed.

    // Should be all.
    EXPECT_FALSE(s4.Step());
  }
}

// Adds a column for the autofill profile's country code.
TEST_F(WebDatabaseMigrationTest, MigrateVersion33ToCurrent) {
  // Initialize the database.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_33.sql")));

  // Verify pre-conditions. These are expectations for version 33 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    EXPECT_FALSE(connection.DoesColumnExist("autofill_profiles",
                                            "country_code"));

    // Check that the country value is the one we expect.
    sql::Statement s(
        connection.GetUniqueStatement("SELECT country FROM autofill_profiles"));

    ASSERT_TRUE(s.Step());
    std::string country = s.ColumnString(0);
    EXPECT_EQ("United States", country);
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    ASSERT_TRUE(connection.DoesColumnExist("autofill_profiles",
                                           "country_code"));

    // Check that the country code is properly converted.
    sql::Statement s(connection.GetUniqueStatement(
        "SELECT country_code FROM autofill_profiles"));

    ASSERT_TRUE(s.Step());
    std::string country_code = s.ColumnString(0);
    EXPECT_EQ("US", country_code);
  }
}

// Cleans up bad country code "UK" in favor of good country code "GB".
TEST_F(WebDatabaseMigrationTest, MigrateVersion34ToCurrent) {
  // Initialize the database.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_34.sql")));

  // Verify pre-conditions. These are expectations for version 34 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    EXPECT_TRUE(connection.DoesColumnExist("autofill_profiles",
                                           "country_code"));

    // Check that the country_code value is the one we expect.
    sql::Statement s(
        connection.GetUniqueStatement("SELECT country_code "
                                      "FROM autofill_profiles"));

    ASSERT_TRUE(s.Step());
    std::string country_code = s.ColumnString(0);
    EXPECT_EQ("UK", country_code);

    // Should have only one.
    ASSERT_FALSE(s.Step());
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    ASSERT_TRUE(connection.DoesColumnExist("autofill_profiles",
                                           "country_code"));

    // Check that the country_code code is properly converted.
    sql::Statement s(connection.GetUniqueStatement(
        "SELECT country_code FROM autofill_profiles"));

    ASSERT_TRUE(s.Step());
    std::string country_code = s.ColumnString(0);
    EXPECT_EQ("GB", country_code);

    // Should have only one.
    ASSERT_FALSE(s.Step());
  }
}

// Cleans up invalid profiles based on more agressive merging.  Filters out
// profiles that are subsets of other profiles, and profiles with invalid email,
// state, and incomplete address.
TEST_F(WebDatabaseMigrationTest, MigrateVersion35ToCurrent) {
  // Initialize the database.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_35.sql")));

  // Verify pre-conditions. These are expectations for version 34 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    EXPECT_FALSE(connection.DoesTableExist("autofill_profiles_trash"));
    ASSERT_TRUE(connection.DoesColumnExist("autofill_profiles", "guid"));

    // Check that there are 6 profiles prior to merge.
    sql::Statement s(
        connection.GetUniqueStatement("SELECT guid FROM autofill_profiles"));
    int i = 0;
    while (s.Step())
      ++i;
    EXPECT_EQ(6, i);
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    ASSERT_TRUE(connection.DoesTableExist("autofill_profiles_trash"));
    ASSERT_TRUE(connection.DoesColumnExist("autofill_profiles_trash", "guid"));
    ASSERT_TRUE(connection.DoesColumnExist("autofill_profiles", "guid"));

    // Verify data in the database after the migration.
    sql::Statement s1(
        connection.GetUniqueStatement(
            "SELECT guid, company_name, address_line_1, address_line_2, "
            "city, state, zipcode, country, date_modified "
            "FROM autofill_profiles"));

    // John Doe.
    ASSERT_TRUE(s1.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000001", s1.ColumnString(0));
    EXPECT_EQ(ASCIIToUTF16("Acme Inc."), s1.ColumnString16(1));
    EXPECT_EQ(ASCIIToUTF16("1 Main Street"), s1.ColumnString16(2));
    EXPECT_EQ(ASCIIToUTF16("Apt 2"), s1.ColumnString16(3));
    EXPECT_EQ(ASCIIToUTF16("San Francisco"), s1.ColumnString16(4));
    EXPECT_EQ(ASCIIToUTF16("CA"), s1.ColumnString16(5));
    EXPECT_EQ(ASCIIToUTF16("94102"), s1.ColumnString16(6));
    EXPECT_EQ(ASCIIToUTF16("United States"), s1.ColumnString16(7));
    EXPECT_EQ(1300131704, s1.ColumnInt64(8));

    // That should be it.
    ASSERT_FALSE(s1.Step());

    // Check that there 5 trashed profile after the merge.
    sql::Statement s2(
        connection.GetUniqueStatement("SELECT guid "
                                      "FROM autofill_profiles_trash"));
    ASSERT_TRUE(s2.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000002", s2.ColumnString(0));

    ASSERT_TRUE(s2.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000003", s2.ColumnString(0));

    ASSERT_TRUE(s2.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000004", s2.ColumnString(0));

    ASSERT_TRUE(s2.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000005", s2.ColumnString(0));

    ASSERT_TRUE(s2.Step());
    EXPECT_EQ("00000000-0000-0000-0000-000000000006", s2.ColumnString(0));

    // That should be it.
    ASSERT_FALSE(s2.Step());
  }
}

// Tests that the |keywords| |last_modified| column gets added to the schema for
// a version 37 database.
TEST_F(WebDatabaseMigrationTest, MigrateVersion37ToCurrent) {
  // This schema is taken from a build prior to the addition of the |keywords|
  // |last_modified| column.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_37.sql")));

  // Verify pre-conditions.  These are expectations for version 37 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Columns existing and not existing before current version.
    ASSERT_TRUE(connection.DoesColumnExist("keywords", "id"));
    ASSERT_FALSE(connection.DoesColumnExist("keywords", "last_modified"));
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // |keywords| |last_modified| column should have been added.
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "id"));
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "last_modified"));
  }
}

// Tests that the |keywords| |sync_guid| column gets added to the schema for
// a version 38 database.
TEST_F(WebDatabaseMigrationTest, MigrateVersion38ToCurrent) {
  // This schema is taken from a build prior to the addition of the |keywords|
  // |sync_guid| column.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_38.sql")));

  // Verify pre-conditions.  These are expectations for version 38 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Columns existing and not existing before current version.
    ASSERT_TRUE(connection.DoesColumnExist("keywords", "id"));
    ASSERT_FALSE(connection.DoesColumnExist("keywords", "sync_guid"));
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // |keywords| |sync_guid| column should have been added.
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "id"));
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "sync_guid"));
  }
}

// Tests that the backup field for the default search provider gets added to
// the meta table of a version 39 database.
TEST_F(WebDatabaseMigrationTest, MigrateVersion39ToCurrent) {
  // This schema is taken from a build prior to the addition of the default
  // search provider backup field to the meta table.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_39.sql")));

  // Verify pre-conditions.  These are expectations for version 39 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 39, 39));

    int64 default_search_provider_id = 0;
    EXPECT_TRUE(meta_table.GetValue(
        "Default Search Provider ID",
        &default_search_provider_id));

    int64 default_search_provider_id_backup = 0;
    EXPECT_FALSE(meta_table.GetValue(
        "Default Search Provider ID Backup",
        &default_search_provider_id_backup));

    std::string default_search_provider_id_backup_signature;
    EXPECT_FALSE(meta_table.GetValue(
        "Default Search Provider ID Backup Signature",
        &default_search_provider_id_backup_signature));
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(
        &connection,
        kCurrentTestedVersionNumber,
        kCurrentTestedVersionNumber));

    int64 default_search_provider_id = 0;
    EXPECT_TRUE(meta_table.GetValue(
        "Default Search Provider ID",
        &default_search_provider_id));

    int64 default_search_provider_id_backup = 0;
    EXPECT_TRUE(meta_table.GetValue(
        "Default Search Provider ID Backup",
        &default_search_provider_id_backup));
    EXPECT_EQ(default_search_provider_id, default_search_provider_id_backup);

    std::string default_search_provider_id_backup_signature;
    EXPECT_TRUE(meta_table.GetValue(
        "Default Search Provider ID Backup Signature",
        &default_search_provider_id_backup_signature));
  }
}

// Tests that the backup field for the default search provider is rewritten
// despite any value in the old version.
TEST_F(WebDatabaseMigrationTest, MigrateVersion40ToCurrent) {
  // This schema is taken from a build after the addition of the default
  // search provider backup field to the meta table. Due to crbug.com/101815
  // the signature was empty in all build between revisions 106214 and 108111.
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_40.sql")));

  // Verify pre-conditions.  These are expectations for version 40 of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 40, 40));

    int64 default_search_provider_id = 0;
    EXPECT_TRUE(meta_table.GetValue(
        "Default Search Provider ID",
        &default_search_provider_id));

    int64 default_search_provider_id_backup = 0;
    EXPECT_TRUE(meta_table.GetValue(
        "Default Search Provider ID Backup",
        &default_search_provider_id_backup));

    std::string default_search_provider_id_backup_signature;
    EXPECT_TRUE(meta_table.GetValue(
        "Default Search Provider ID Backup Signature",
        &default_search_provider_id_backup_signature));
    EXPECT_TRUE(default_search_provider_id_backup_signature.empty());
  }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  {
    WebDatabase db;
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Connection connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(
        &connection,
        kCurrentTestedVersionNumber,
        kCurrentTestedVersionNumber));

    int64 default_search_provider_id = 0;
    EXPECT_TRUE(meta_table.GetValue(
        "Default Search Provider ID",
        &default_search_provider_id));
    EXPECT_NE(0, default_search_provider_id);

    int64 default_search_provider_id_backup = 0;
    EXPECT_TRUE(meta_table.GetValue(
        "Default Search Provider ID Backup",
        &default_search_provider_id_backup));
    EXPECT_EQ(default_search_provider_id, default_search_provider_id_backup);

    std::string default_search_provider_id_backup_signature;
    EXPECT_TRUE(meta_table.GetValue(
        "Default Search Provider ID Backup Signature",
        &default_search_provider_id_backup_signature));
    EXPECT_FALSE(default_search_provider_id_backup_signature.empty());
  }
}

