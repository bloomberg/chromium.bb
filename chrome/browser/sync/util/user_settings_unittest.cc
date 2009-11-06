// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE entry.

#include <string>

#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/sync/syncable/directory_manager.h"
#include "chrome/browser/sync/util/character_set_converters.h"
#include "chrome/browser/sync/util/user_settings.h"
#include "chrome/browser/sync/util/query_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::FilePathToUTF8;
using browser_sync::UserSettings;

static const FilePath::CharType kV10UserSettingsDB[] =
    FILE_PATH_LITERAL("Version10Settings.sqlite3");
static const FilePath::CharType kOldStyleSyncDataDB[] =
    FILE_PATH_LITERAL("OldStyleSyncData.sqlite3");

class UserSettingsTest : public testing::Test {
 public:
  UserSettingsTest() : sync_data_("Some sync data") { }

  // Creates and populates the V10 database files within
  // |destination_directory|.
  void SetUpVersion10Databases(const FilePath& destination_directory) {
    sqlite3* primer_handle = NULL;
    v10_user_setting_db_path_ =
        destination_directory.Append(FilePath(kV10UserSettingsDB));
    ASSERT_EQ(SQLITE_OK, SqliteOpen(v10_user_setting_db_path_, &primer_handle));
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

    // Create and populate version table.
    ExecOrDie(primer_handle, "CREATE TABLE db_version ( version )");
    ExecOrDie(primer_handle, "INSERT INTO db_version values ( ? )", 10);
    // Create shares table.
    ExecOrDie(primer_handle, "CREATE TABLE shares"
              " (email, share_name, file_name,"
              "  PRIMARY KEY(email, share_name) ON CONFLICT REPLACE)");
    // Populate a share.
    ExecOrDie(primer_handle, "INSERT INTO shares values ( ?, ?, ?)",
              "foo@foo.com", "foo@foo.com",
              FilePathToUTF8(old_style_sync_data_path_));
    sqlite3_close(primer_handle);
  }

  const std::string& sync_data() const { return sync_data_; }
  const FilePath& v10_user_setting_db_path() const {
    return v10_user_setting_db_path_;
  }
  const FilePath& old_style_sync_data_path() const {
    return old_style_sync_data_path_;
  }

 private:
  FilePath v10_user_setting_db_path_;
  FilePath old_style_sync_data_path_;
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
  ASSERT_EQ(SQLITE_OK, SqliteOpen(v10_user_setting_db_path(), &handle));

  // Note that we don't use ScopedStatement to avoid closing the sqlite handle
  // before finalizing the statement.
  sqlite3_stmt* version_query =
      PrepareQuery(handle, "SELECT version FROM db_version");
  ASSERT_EQ(SQLITE_ROW, sqlite3_step(version_query));
  const int version = sqlite3_column_int(version_query, 0);
  EXPECT_EQ(11, version);
  sqlite3_finalize(version_query);

  EXPECT_FALSE(file_util::PathExists(old_style_sync_data_path()));

  FilePath new_style_path = temp_dir.path().Append(
      syncable::DirectoryManager::GetSyncDataDatabaseFilename());

  std::string contents;
  ASSERT_TRUE(file_util::ReadFileToString(new_style_path, &contents));
  EXPECT_TRUE(sync_data() == contents);
  sqlite3_close(handle);
}
