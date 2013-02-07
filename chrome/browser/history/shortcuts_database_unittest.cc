// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/shortcuts_database.h"
#include "chrome/test/base/testing_profile.h"
#include "sql/statement.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace history {

struct ShortcutsDatabaseTestInfo {
  std::string guid;
  std::string url;
  std::string title;  // The text that orginally was searched for.
  std::string contents;
  std::string contents_class;
  std::string description;
  std::string description_class;
  int typed_count;
  int days_from_now;
} shortcut_test_db[] = {
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880DF",
    "http://www.google.com/", "goog",
    "Google", "0,1,4,0", "Google", "0,3,4,1", 100, 1 },
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E0",
    "http://slashdot.org/", "slash",
    "slashdot.org", "0,3,5,1",
    "Slashdot - News for nerds, stuff that matters", "0,2,5,0", 100, 0},
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E1",
    "http://slashdot.org/", "news",
    "slashdot.org", "0,1",
    "Slashdot - News for nerds, stuff that matters", "0,0,11,2,15,0", 5, 0},
};

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
  db_ = new ShortcutsDatabase(profile_.get());
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
  return ShortcutsBackend::Shortcut(info.guid, ASCIIToUTF16(info.title),
      GURL(info.url), ASCIIToUTF16(info.contents),
      AutocompleteMatch::ClassificationsFromString(info.contents_class),
      ASCIIToUTF16(info.description),
      AutocompleteMatch::ClassificationsFromString(info.description_class),
      base::Time::Now() - base::TimeDelta::FromDays(info.days_from_now),
      info.typed_count);
}

void ShortcutsDatabaseTest::AddAll() {
  ClearDB();
  for (size_t i = 0; i < arraysize(shortcut_test_db); ++i) {
    db_->AddShortcut(ShortcutFromTestInfo(shortcut_test_db[i]));
  }
  EXPECT_EQ(arraysize(shortcut_test_db), CountRecords());
}

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
  shortcut.contents = ASCIIToUTF16("gro.todhsals");
  EXPECT_TRUE(db_->UpdateShortcut(shortcut));
  ShortcutsDatabase::GuidToShortcutMap shortcuts;
  EXPECT_TRUE(db_->LoadShortcuts(&shortcuts));
  ShortcutsDatabase::GuidToShortcutMap::iterator it =
      shortcuts.find(shortcut.id);
  EXPECT_TRUE(it != shortcuts.end());
  EXPECT_TRUE(it->second.contents == shortcut.contents);
}

TEST_F(ShortcutsDatabaseTest, DeleteShortcutsWithIds) {
  AddAll();
  std::vector<std::string> shortcut_ids;
  shortcut_ids.push_back(shortcut_test_db[0].guid);
  shortcut_ids.push_back(shortcut_test_db[2].guid);
  EXPECT_TRUE(db_->DeleteShortcutsWithIds(shortcut_ids));
  EXPECT_EQ(arraysize(shortcut_test_db) - 2, CountRecords());

  ShortcutsDatabase::GuidToShortcutMap shortcuts;
  EXPECT_TRUE(db_->LoadShortcuts(&shortcuts));

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
  EXPECT_TRUE(db_->LoadShortcuts(&shortcuts));

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
  EXPECT_TRUE(db_->LoadShortcuts(&shortcuts));

  for (size_t i = 0; i < arraysize(shortcut_test_db); ++i) {
    SCOPED_TRACE(base::StringPrintf("Looking for shortcut #%d",
        static_cast<int>(i)));
    EXPECT_TRUE(shortcuts.find(shortcut_test_db[i].guid) != shortcuts.end());
  }
}

TEST_F(ShortcutsDatabaseTest, DeleteAllShortcuts) {
  AddAll();
  ShortcutsDatabase::GuidToShortcutMap shortcuts;
  EXPECT_TRUE(db_->LoadShortcuts(&shortcuts));
  EXPECT_EQ(arraysize(shortcut_test_db), shortcuts.size());
  EXPECT_TRUE(db_->DeleteAllShortcuts());
  EXPECT_TRUE(db_->LoadShortcuts(&shortcuts));
  EXPECT_EQ(0U, shortcuts.size());
}

}  // namespace history
