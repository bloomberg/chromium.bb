// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/history/shortcuts_database.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"

#include "testing/gtest/include/gtest/gtest.h"


// Helpers --------------------------------------------------------------------

namespace {

struct ShortcutsDatabaseTestInfo {
  std::string guid;
  std::string text;
  std::string fill_into_edit;
  std::string destination_url;
  std::string contents;
  std::string contents_class;
  std::string stripped_contents_class;  // |contents_class| with matches removed
  std::string description;
  std::string description_class;
  std::string stripped_description_class;
  content::PageTransition transition;
  AutocompleteMatch::Type type;
  std::string keyword;
  int days_from_now;
  int number_of_hits;
} shortcut_test_db[] = {
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880DF", "goog", "www.google.com",
    "http://www.google.com/", "Google", "0,1,4,0", "0,1,4,0", "Google",
    "0,3,4,1", "0,1", content::PAGE_TRANSITION_GENERATED,
    AutocompleteMatchType::SEARCH_HISTORY, "google.com", 1, 100, },
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E0", "slash", "slashdot.org",
    "http://slashdot.org/", "slashdot.org", "0,3,5,1", "0,1",
    "Slashdot - News for nerds, stuff that matters", "0,2,5,0", "0,0",
    content::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 0,
    100},
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E1", "news", "slashdot.org",
    "http://slashdot.org/", "slashdot.org", "0,1", "0,1",
    "Slashdot - News for nerds, stuff that matters", "0,0,11,2,15,0", "0,0",
    content::PAGE_TRANSITION_LINK, AutocompleteMatchType::HISTORY_TITLE, "", 0,
    5},
};

typedef testing::Test ShortcutsDatabaseMigrationTest;

// Checks that the database at |db| has the version 2 columns iff |is_v2|.
void CheckV2ColumnExistence(const base::FilePath& db_path, bool is_v2) {
  sql::Connection connection;
  ASSERT_TRUE(connection.Open(db_path));
  EXPECT_EQ(is_v2, connection.DoesColumnExist("omni_box_shortcuts",
                                              "fill_into_edit"));
  EXPECT_EQ(is_v2, connection.DoesColumnExist("omni_box_shortcuts",
                                              "transition"));
  EXPECT_EQ(is_v2, connection.DoesColumnExist("omni_box_shortcuts", "type"));
  EXPECT_EQ(is_v2, connection.DoesColumnExist("omni_box_shortcuts", "keyword"));
}

}  // namespace

namespace history {


// ShortcutsDatabaseTest ------------------------------------------------------

class ShortcutsDatabaseTest : public testing::Test {
 public:
  virtual void SetUp();
  virtual void TearDown();

  void ClearDB();
  size_t CountRecords() const;

  ShortcutsBackend::Shortcut ShortcutFromTestInfo(
      const ShortcutsDatabaseTestInfo& info);

  void AddAll();

  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<ShortcutsDatabase> db_;
};

void ShortcutsDatabaseTest::SetUp() {
  profile_.reset(new TestingProfile());
  db_ = new ShortcutsDatabase(
      profile_->GetPath().Append(chrome::kShortcutsDatabaseName));
  ASSERT_TRUE(db_->Init());
  ClearDB();
}

void ShortcutsDatabaseTest::TearDown() {
  db_ = NULL;
}

void ShortcutsDatabaseTest::ClearDB() {
  sql::Statement
      s(db_->db_.GetUniqueStatement("DELETE FROM omni_box_shortcuts"));
  EXPECT_TRUE(s.Run());
}

size_t ShortcutsDatabaseTest::CountRecords() const {
  sql::Statement s(db_->db_.GetUniqueStatement(
      "SELECT count(*) FROM omni_box_shortcuts"));
  EXPECT_TRUE(s.Step());
  return static_cast<size_t>(s.ColumnInt(0));
}

ShortcutsBackend::Shortcut ShortcutsDatabaseTest::ShortcutFromTestInfo(
    const ShortcutsDatabaseTestInfo& info) {
  return ShortcutsBackend::Shortcut(
      info.guid, ASCIIToUTF16(info.text),
      ShortcutsBackend::Shortcut::MatchCore(
          ASCIIToUTF16(info.fill_into_edit), GURL(info.destination_url),
          ASCIIToUTF16(info.contents),
          AutocompleteMatch::ClassificationsFromString(info.contents_class),
          ASCIIToUTF16(info.description),
          AutocompleteMatch::ClassificationsFromString(info.description_class),
          info.transition, info.type, ASCIIToUTF16(info.keyword)),
      base::Time::Now() - base::TimeDelta::FromDays(info.days_from_now),
      info.number_of_hits);
}

void ShortcutsDatabaseTest::AddAll() {
  ClearDB();
  for (size_t i = 0; i < arraysize(shortcut_test_db); ++i)
    db_->AddShortcut(ShortcutFromTestInfo(shortcut_test_db[i]));
  EXPECT_EQ(arraysize(shortcut_test_db), CountRecords());
}


// Actual tests ---------------------------------------------------------------

TEST_F(ShortcutsDatabaseTest, AddShortcut) {
  ClearDB();
  EXPECT_EQ(0U, CountRecords());
  EXPECT_TRUE(db_->AddShortcut(ShortcutFromTestInfo(shortcut_test_db[0])));
  EXPECT_EQ(1U, CountRecords());
  EXPECT_TRUE(db_->AddShortcut(ShortcutFromTestInfo(shortcut_test_db[1])));
  EXPECT_EQ(2U, CountRecords());
  EXPECT_TRUE(db_->AddShortcut(ShortcutFromTestInfo(shortcut_test_db[2])));
  EXPECT_EQ(3U, CountRecords());
}

TEST_F(ShortcutsDatabaseTest, UpdateShortcut) {
  AddAll();
  ShortcutsBackend::Shortcut shortcut(
      ShortcutFromTestInfo(shortcut_test_db[1]));
  shortcut.match_core.contents = ASCIIToUTF16("gro.todhsals");
  EXPECT_TRUE(db_->UpdateShortcut(shortcut));
  ShortcutsDatabase::GuidToShortcutMap shortcuts;
  db_->LoadShortcuts(&shortcuts);
  ShortcutsDatabase::GuidToShortcutMap::const_iterator it(
      shortcuts.find(shortcut.id));
  EXPECT_TRUE(it != shortcuts.end());
  EXPECT_TRUE(it->second.match_core.contents == shortcut.match_core.contents);
}

TEST_F(ShortcutsDatabaseTest, DeleteShortcutsWithIds) {
  AddAll();
  std::vector<std::string> shortcut_ids;
  shortcut_ids.push_back(shortcut_test_db[0].guid);
  shortcut_ids.push_back(shortcut_test_db[2].guid);
  EXPECT_TRUE(db_->DeleteShortcutsWithIds(shortcut_ids));
  EXPECT_EQ(arraysize(shortcut_test_db) - 2, CountRecords());

  ShortcutsDatabase::GuidToShortcutMap shortcuts;
  db_->LoadShortcuts(&shortcuts);

  ShortcutsDatabase::GuidToShortcutMap::iterator it =
      shortcuts.find(shortcut_test_db[0].guid);
  EXPECT_TRUE(it == shortcuts.end());

  it = shortcuts.find(shortcut_test_db[1].guid);
  EXPECT_TRUE(it != shortcuts.end());

  it = shortcuts.find(shortcut_test_db[2].guid);
  EXPECT_TRUE(it == shortcuts.end());
}

TEST_F(ShortcutsDatabaseTest, DeleteShortcutsWithUrl) {
  AddAll();

  EXPECT_TRUE(db_->DeleteShortcutsWithUrl("http://slashdot.org/"));
  EXPECT_EQ(arraysize(shortcut_test_db) - 2, CountRecords());

  ShortcutsDatabase::GuidToShortcutMap shortcuts;
  db_->LoadShortcuts(&shortcuts);

  ShortcutsDatabase::GuidToShortcutMap::iterator it =
      shortcuts.find(shortcut_test_db[0].guid);
  EXPECT_TRUE(it != shortcuts.end());

  it = shortcuts.find(shortcut_test_db[1].guid);
  EXPECT_TRUE(it == shortcuts.end());

  it = shortcuts.find(shortcut_test_db[2].guid);
  EXPECT_TRUE(it == shortcuts.end());
}

TEST_F(ShortcutsDatabaseTest, LoadShortcuts) {
  AddAll();
  ShortcutsDatabase::GuidToShortcutMap shortcuts;
  db_->LoadShortcuts(&shortcuts);

  for (size_t i = 0; i < arraysize(shortcut_test_db); ++i) {
    SCOPED_TRACE(base::StringPrintf("Comparing shortcut #%" PRIuS, i));
    ShortcutsDatabase::GuidToShortcutMap::const_iterator it(
        shortcuts.find(shortcut_test_db[i].guid));
    ASSERT_TRUE(it != shortcuts.end());
    const ShortcutsBackend::Shortcut& shortcut = it->second;
    EXPECT_EQ(ASCIIToUTF16(shortcut_test_db[i].text), shortcut.text);
    EXPECT_EQ(ASCIIToUTF16(shortcut_test_db[i].fill_into_edit),
              shortcut.match_core.fill_into_edit);
    EXPECT_EQ(shortcut_test_db[i].destination_url,
              shortcut.match_core.destination_url.spec());
    EXPECT_EQ(ASCIIToUTF16(shortcut_test_db[i].contents),
              shortcut.match_core.contents);
    EXPECT_EQ(shortcut_test_db[i].stripped_contents_class,
              AutocompleteMatch::ClassificationsToString(
                  shortcut.match_core.contents_class));
    EXPECT_EQ(ASCIIToUTF16(shortcut_test_db[i].description),
              shortcut.match_core.description);
    EXPECT_EQ(shortcut_test_db[i].stripped_description_class,
              AutocompleteMatch::ClassificationsToString(
                  shortcut.match_core.description_class));
    EXPECT_EQ(shortcut_test_db[i].transition, shortcut.match_core.transition);
    EXPECT_EQ(shortcut_test_db[i].type, shortcut.match_core.type);
    EXPECT_EQ(ASCIIToUTF16(shortcut_test_db[i].keyword),
              shortcut.match_core.keyword);
    EXPECT_EQ(shortcut_test_db[i].number_of_hits, shortcut.number_of_hits);
    // We don't bother trying to check the |days_from_now| field, since dealing
    // with times in unittests is annoying.
  }
}

TEST_F(ShortcutsDatabaseTest, DeleteAllShortcuts) {
  AddAll();
  ShortcutsDatabase::GuidToShortcutMap shortcuts;
  db_->LoadShortcuts(&shortcuts);
  EXPECT_EQ(arraysize(shortcut_test_db), shortcuts.size());
  EXPECT_TRUE(db_->DeleteAllShortcuts());
  db_->LoadShortcuts(&shortcuts);
  EXPECT_EQ(0U, shortcuts.size());
}

TEST(ShortcutsDatabaseMigrationTest, MigrateV1ToV2) {
  // Open the v1 test file and use it to create a test database in a temp dir.
  base::FilePath sql_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &sql_path));
  sql_path = sql_path.AppendASCII("History").AppendASCII("Shortcuts.v1.sql");
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath db_path(temp_dir.path().AppendASCII("TestShortcuts.db"));
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path, sql_path));

  CheckV2ColumnExistence(db_path, false);

  // Create a ShortcutsDatabase from the test database, which will migrate the
  // test database to the current version.
  {
    scoped_refptr<ShortcutsDatabase> db(new ShortcutsDatabase(db_path));
    db->Init();
  }

  CheckV2ColumnExistence(db_path, true);

  // Check the values in each of the new columns.
  sql::Connection connection;
  ASSERT_TRUE(connection.Open(db_path));
  sql::Statement statement(connection.GetUniqueStatement(
      "SELECT fill_into_edit, url, transition, type, keyword "
      "FROM omni_box_shortcuts"));
  ASSERT_TRUE(statement.is_valid());
  while (statement.Step()) {
    // |fill_into_edit| should have been copied from the |url|.
    EXPECT_EQ(statement.ColumnString(1), statement.ColumnString(0));

    // The other three columns have default values.
    EXPECT_EQ(content::PAGE_TRANSITION_TYPED,
              static_cast<content::PageTransition>(statement.ColumnInt(2)));
    EXPECT_EQ(AutocompleteMatchType::HISTORY_TITLE,
              static_cast<AutocompleteMatch::Type>(statement.ColumnInt(3)));
    EXPECT_TRUE(statement.ColumnString(4).empty());
  }
  EXPECT_TRUE(statement.Succeeded());
}

}  // namespace history
