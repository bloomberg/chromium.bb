// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>
#include <string>

#include "app/sql/statement.h"
#include "base/file_util.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/encryptor.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/util/user_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::numeric_limits;

namespace {

const FilePath::CharType kV10UserSettingsDB[] =
    FILE_PATH_LITERAL("Version10Settings.sqlite3");
const FilePath::CharType kV11UserSettingsDB[] =
    FILE_PATH_LITERAL("Version11Settings.sqlite3");
const FilePath::CharType kOldStyleSyncDataDB[] =
    FILE_PATH_LITERAL("OldStyleSyncData.sqlite3");

}  // namespace

class UserSettingsTest : public testing::Test {
 public:
  UserSettingsTest() : sync_data_("Some sync data") {}

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
    v10_user_setting_db_path_ =
        destination_directory.Append(FilePath(kV10UserSettingsDB));

    sql::Connection db;
    ASSERT_TRUE(db.Open(v10_user_setting_db_path_));

    old_style_sync_data_path_ =
        destination_directory.Append(FilePath(kOldStyleSyncDataDB));

    ASSERT_EQ(sync_data_.length(),
              static_cast<size_t>(file_util::WriteFile(
                  old_style_sync_data_path_, sync_data_.data(),
                  sync_data_.length())));

    // Create settings table.
    ASSERT_TRUE(db.Execute(
        "CREATE TABLE settings (email, key, value,  PRIMARY KEY(email, key)"
        " ON CONFLICT REPLACE)"));

    // Add a blank signin table.
    ASSERT_TRUE(db.Execute(
        "CREATE TABLE signin_types (signin, signin_type)"));

    // Create and populate version table.
    ASSERT_TRUE(db.Execute("CREATE TABLE db_version (version)"));
    {
      const char* query = "INSERT INTO db_version VALUES(?)";
      sql::Statement s(db.GetUniqueStatement(query));
      if (!s)
        LOG(FATAL) << query << "\n" << db.GetErrorMessage();

      s.BindInt(0, 10);
      if (!s.Run())
        LOG(FATAL) << query << "\n" << db.GetErrorMessage();
    }

    // Create shares table.
    ASSERT_TRUE(db.Execute(
        "CREATE TABLE shares (email, share_name, file_name,"
        " PRIMARY KEY(email, share_name) ON CONFLICT REPLACE)"));
    // Populate a share.
    {
      const char* query = "INSERT INTO shares VALUES(?, ?, ?)";
      sql::Statement s(db.GetUniqueStatement(query));
      if (!s)
        LOG(FATAL) << query << "\n" << db.GetErrorMessage();

      s.BindString(0, "foo@foo.com");
      s.BindString(1, "foo@foo.com");
#if defined(OS_WIN)
      s.BindString(2, WideToUTF8(old_style_sync_data_path_.value()));
#elif defined(OS_POSIX)
      s.BindString(2, old_style_sync_data_path_.value());
#endif
      if (!s.Run())
        LOG(FATAL) << query << "\n" << db.GetErrorMessage();
    }
  }

   // Creates and populates the V11 database file within
  // |destination_directory|.
  void SetUpVersion11Database(const FilePath& destination_directory) {
    v11_user_setting_db_path_ =
        destination_directory.Append(FilePath(kV11UserSettingsDB));

    sql::Connection db;
    ASSERT_TRUE(db.Open(v11_user_setting_db_path_));

    // Create settings table.
    ASSERT_TRUE(db.Execute(
        "CREATE TABLE settings (email, key, value, PRIMARY KEY(email, key)"
        " ON CONFLICT REPLACE)"));

    // Create and populate version table.
    ASSERT_TRUE(db.Execute("CREATE TABLE db_version (version)"));
    {
      const char* query = "INSERT INTO db_version VALUES(?)";
      sql::Statement s(db.GetUniqueStatement(query));
      if (!s)
        LOG(FATAL) << query << "\n" << db.GetErrorMessage();

      s.BindInt(0, 11);
      if (!s.Run())
        LOG(FATAL) << query << "\n" << db.GetErrorMessage();
    }

    ASSERT_TRUE(db.Execute(
        "CREATE TABLE signin_types (signin, signin_type)"));
    {
      const char* query = "INSERT INTO signin_types VALUES(?, ?)";
      sql::Statement s(db.GetUniqueStatement(query));
      if (!s)
        LOG(FATAL) << query << "\n" << db.GetErrorMessage();

      s.BindString(0, "test");
      s.BindString(1, "test");
      if (!s.Run())
        LOG(FATAL) << query << "\n" << db.GetErrorMessage();
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
    browser_sync::UserSettings settings;
    settings.Init(v10_user_setting_db_path());
  }

  // Now poke around using sqlite to see if UserSettings migrated properly.
  sql::Connection db;
  ASSERT_TRUE(db.Open(v10_user_setting_db_path()));

  // Note that we don't use ScopedStatement to avoid closing the sqlite handle
  // before finalizing the statement.
  {
    const char* query = "SELECT version FROM db_version";
    sql::Statement version_query(db.GetUniqueStatement(query));
    if (!version_query)
      LOG(FATAL) << query << "\n" << db.GetErrorMessage();

    ASSERT_TRUE(version_query.Step());
    const int version = version_query.ColumnInt(0);
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
    browser_sync::UserSettings settings;
    settings.Init(v11_user_setting_db_path());
  }
  sql::Connection db;
  ASSERT_TRUE(db.Open(v11_user_setting_db_path()));

  {
    const char* query = "SELECT version FROM db_version";
    sql::Statement version_query(db.GetUniqueStatement(query));
    if (!version_query)
      LOG(FATAL) << query << "\n" << db.GetErrorMessage();

    ASSERT_TRUE(version_query.Step());
    const int version = version_query.ColumnInt(0);
    EXPECT_GE(version, 12);

    const char* query2 = "SELECT name FROM sqlite_master "
                         "WHERE type='table' AND name='signin_types'";
    sql::Statement table_query(db.GetUniqueStatement(query2));
    if (!table_query)
      LOG(FATAL) << query2 << "\n" << db.GetErrorMessage();

    ASSERT_FALSE(table_query.Step());
  }
}

TEST_F(UserSettingsTest, APEncode) {
  std::string test;
  char i;
  for (i = numeric_limits<char>::min(); i < numeric_limits<char>::max(); ++i)
    test.push_back(i);
  test.push_back(i);
  const std::string encoded = browser_sync::APEncode(test);
  const std::string decoded = browser_sync::APDecode(encoded);
  ASSERT_EQ(test, decoded);
}

TEST_F(UserSettingsTest, PersistEmptyToken) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  browser_sync::UserSettings settings;
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
  browser_sync::UserSettings settings;
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
