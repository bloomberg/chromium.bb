// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <string>

#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/util/user_settings.h"
#include "chrome/common/sqlite_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::APEncode;
using browser_sync::APDecode;
using browser_sync::ExecOrDie;
using browser_sync::UserSettings;

using std::numeric_limits;

static const FilePath::CharType kV10UserSettingsDB[] =
    FILE_PATH_LITERAL("Version10Settings.sqlite3");
static const FilePath::CharType kV11UserSettingsDB[] =
    FILE_PATH_LITERAL("Version11Settings.sqlite3");
static const FilePath::CharType kOldStyleSyncDataDB[] =
    FILE_PATH_LITERAL("OldStyleSyncData.sqlite3");

class UserSettingsTest : public testing::Test {
 public:
  UserSettingsTest() : sync_data_("Some sync data") { }

  virtual void SetUp() {
#if defined(OS_MACOSX)
    // Need to mock the Keychain for unit tests on Mac to avoid possible
    // blocking UI.  |SetAuthTokenForService| uses Encryptor.
    Encryptor::UseMockKeychain(true);
#endif
  }

  // Creates and populates the V10 database files within
  // |destination_directory|.
  void SetUpVersion10Databases(const FilePath& destination_directory) {
    sqlite3* primer_handle = NULL;
    v10_user_setting_db_path_ =
        destination_directory.Append(FilePath(kV10UserSettingsDB));
    ASSERT_EQ(SQLITE_OK, sqlite_utils::OpenSqliteDb(v10_user_setting_db_path_,
                                                    &primer_handle));
    sqlite_utils::scoped_sqlite_db_ptr db(primer_handle);

    old_style_sync_data_path_ =
        destination_directory.Append(FilePath(kOldStyleSyncDataDB));

    ASSERT_EQ(sync_data_.length(),
              static_cast<size_t>(file_util::WriteFile(
                  old_style_sync_data_path_, sync_data_.data(),
                  sync_data_.length())));

    // Create settings table.
    ExecOrDie(primer_handle, "CREATE TABLE settings"
              " (email, key, value, "
              "  PRIMARY KEY(email, key) ON CONFLICT REPLACE)");
    // Add a blank signin table.
    ExecOrDie(primer_handle, "CREATE TABLE signin_types"
              " (signin, signin_type)");
    // Create and populate version table.
    ExecOrDie(primer_handle, "CREATE TABLE db_version ( version )");
    {
      SQLStatement statement;
      const char query[] = "INSERT INTO db_version values ( ? )";
      statement.prepare(primer_handle, query);
      statement.bind_int(0, 10);
      if (SQLITE_DONE != statement.step()) {
        LOG(FATAL) << query << "\n" << sqlite3_errmsg(primer_handle);
      }
    }
    // Create shares table.
    ExecOrDie(primer_handle, "CREATE TABLE shares"
              " (email, share_name, file_name,"
              "  PRIMARY KEY(email, share_name) ON CONFLICT REPLACE)");
    // Populate a share.
    {
      SQLStatement statement;
      const char query[] = "INSERT INTO shares values ( ?, ?, ? )";
      statement.prepare(primer_handle, query);
      statement.bind_string(0, "foo@foo.com");
      statement.bind_string(1, "foo@foo.com");
#if defined(OS_WIN)
      statement.bind_string(2, WideToUTF8(old_style_sync_data_path_.value()));
#elif defined(OS_POSIX)
      statement.bind_string(2, old_style_sync_data_path_.value());
#endif
      if (SQLITE_DONE != statement.step()) {
        LOG(FATAL) << query << "\n" << sqlite3_errmsg(primer_handle);
      }
    }
  }

   // Creates and populates the V11 database file within
  // |destination_directory|.
  void SetUpVersion11Database(const FilePath& destination_directory) {
    sqlite3* primer_handle = NULL;
    v11_user_setting_db_path_ =
        destination_directory.Append(FilePath(kV11UserSettingsDB));
    ASSERT_EQ(SQLITE_OK, sqlite_utils::OpenSqliteDb(v11_user_setting_db_path_,
                                                    &primer_handle));
    sqlite_utils::scoped_sqlite_db_ptr db(primer_handle);

    // Create settings table.
    ExecOrDie(primer_handle, "CREATE TABLE settings"
              " (email, key, value, "
              "  PRIMARY KEY(email, key) ON CONFLICT REPLACE)");

    // Create and populate version table.
    ExecOrDie(primer_handle, "CREATE TABLE db_version ( version )");
    {
      SQLStatement statement;
      const char query[] = "INSERT INTO db_version values ( ? )";
      statement.prepare(primer_handle, query);
      statement.bind_int(0, 11);
      if (SQLITE_DONE != statement.step()) {
        LOG(FATAL) << query << "\n" << sqlite3_errmsg(primer_handle);
      }
    }

    ExecOrDie(primer_handle, "CREATE TABLE signin_types"
              " (signin, signin_type)");

    {
      SQLStatement statement;
      const char query[] = "INSERT INTO signin_types values ( ?, ? )";
      statement.prepare(primer_handle, query);
      statement.bind_string(0, "test");
      statement.bind_string(1, "test");
      if (SQLITE_DONE != statement.step()) {
        LOG(FATAL) << query << "\n" << sqlite3_errmsg(primer_handle);
      }
    }
  }

  const std::string& sync_data() const { return sync_data_; }
  const FilePath& v10_user_setting_db_path() const {
    return v10_user_setting_db_path_;
  }
  const FilePath& v11_user_setting_db_path() const {
    return v11_user_setting_db_path_;
  }
  const FilePath& old_style_sync_data_path() const {
    return old_style_sync_data_path_;
  }

 private:
  FilePath v10_user_setting_db_path_;
  FilePath old_style_sync_data_path_;

  FilePath v11_user_setting_db_path_;

  std::string sync_data_;
};

TEST_F(UserSettingsTest, MigrateFromV10ToV11) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  SetUpVersion10Databases(temp_dir.path());
  {
    // Create a UserSettings, which should trigger migration code. We do this
    // inside a scoped block so it closes itself and we can poke around to see
    // what happened later.
    UserSettings settings;
    settings.Init(v10_user_setting_db_path());
  }

  // Now poke around using sqlite to see if UserSettings migrated properly.
  sqlite3* handle = NULL;
  ASSERT_EQ(SQLITE_OK, sqlite_utils::OpenSqliteDb(v10_user_setting_db_path(),
                                                  &handle));
  sqlite_utils::scoped_sqlite_db_ptr db(handle);

  // Note that we don't use ScopedStatement to avoid closing the sqlite handle
  // before finalizing the statement.
  {
    SQLStatement version_query;
    version_query.prepare(handle, "SELECT version FROM db_version");
    ASSERT_EQ(SQLITE_ROW, version_query.step());
    const int version = version_query.column_int(0);
    EXPECT_GE(version, 11);
  }

  EXPECT_FALSE(file_util::PathExists(old_style_sync_data_path()));

  FilePath new_style_path = temp_dir.path().Append(
      syncable::DirectoryManager::GetSyncDataDatabaseFilename());

  std::string contents;
  ASSERT_TRUE(file_util::ReadFileToString(new_style_path, &contents));
  EXPECT_TRUE(sync_data() == contents);
}

TEST_F(UserSettingsTest, MigrateFromV11ToV12) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  SetUpVersion11Database(temp_dir.path());
  {
    UserSettings settings;
    settings.Init(v11_user_setting_db_path());
  }
  sqlite3* handle = NULL;
  ASSERT_EQ(SQLITE_OK, sqlite_utils::OpenSqliteDb(v11_user_setting_db_path(),
                                                  &handle));
  sqlite_utils::scoped_sqlite_db_ptr db(handle);

  {
    SQLStatement version_query;
    version_query.prepare(handle, "SELECT version FROM db_version");
    ASSERT_EQ(SQLITE_ROW, version_query.step());
    const int version = version_query.column_int(0);
    EXPECT_GE(version, 12);

    SQLStatement table_query;
    table_query.prepare(handle, "SELECT name FROM sqlite_master "
        "WHERE type='table' AND name='signin_types'");
    ASSERT_NE(SQLITE_ROW, table_query.step());
  }
}

TEST_F(UserSettingsTest, APEncode) {
  std::string test;
  char i;
  for (i = numeric_limits<char>::min(); i < numeric_limits<char>::max(); ++i)
    test.push_back(i);
  test.push_back(i);
  const std::string encoded = APEncode(test);
  const std::string decoded = APDecode(encoded);
  ASSERT_EQ(test, decoded);
}

TEST_F(UserSettingsTest, PersistEmptyToken) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  UserSettings settings;
  settings.Init(temp_dir.path().AppendASCII("UserSettings.sqlite3"));
  settings.SetAuthTokenForService("username", "service", "");
  std::string username;
  std::string token;
  ASSERT_TRUE(settings.GetLastUserAndServiceToken("service", &username,
      &token));
  EXPECT_EQ("", token);
  EXPECT_EQ("username", username);
}

TEST_F(UserSettingsTest, PersistNonEmptyToken) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  UserSettings settings;
  settings.Init(temp_dir.path().AppendASCII("UserSettings.sqlite3"));
  settings.SetAuthTokenForService("username", "service",
      "oonetuhasonteuhasonetuhasonetuhasonetuhasouhasonetuhasonetuhasonetuhah"
      "oonetuhasonteuhasonetuhasonetuhasonetuhasouhasonetuhasonetuhasonetuhah"
      "oonetuhasonteuhasonetuhasonetuhasonetuhasouhasonetuhasonetuhasonetuhah");
  std::string username;
  std::string token;
  ASSERT_TRUE(settings.GetLastUserAndServiceToken("service", &username,
      &token));
  EXPECT_EQ(
      "oonetuhasonteuhasonetuhasonetuhasonetuhasouhasonetuhasonetuhasonetuhah"
      "oonetuhasonteuhasonetuhasonetuhasonetuhasouhasonetuhasonetuhasonetuhah"
      "oonetuhasonteuhasonetuhasonetuhasonetuhasouhasonetuhasonetuhasonetuhah",
      token);
  EXPECT_EQ("username", username);
}
