// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/shortcuts_provider_shortcut.h"
#include "chrome/browser/history/shortcuts_database.h"
#include "chrome/common/guid.h"
#include "sql/statement.h"

#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;
using shortcuts_provider::Shortcut;
using shortcuts_provider::ShortcutMap;

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
  void SetUp();
  void TearDown();

  void ClearDB();
  size_t CountRecords() const;

  Shortcut ShortcutFromTestInfo(const ShortcutsDatabaseTestInfo& info);

  void AddAll();

  ScopedTempDir temp_dir_;
  scoped_refptr<ShortcutsDatabase> db_;
};

void ShortcutsDatabaseTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  db_ = new ShortcutsDatabase(temp_dir_.path());
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

Shortcut ShortcutsDatabaseTest::ShortcutFromTestInfo(
    const ShortcutsDatabaseTestInfo& info) {
  Time visit_time = Time::Now() - TimeDelta::FromDays(info.days_from_now);
  return Shortcut(info.guid,
                  ASCIIToUTF16(info.title),
                  ASCIIToUTF16(info.url),
                  ASCIIToUTF16(info.contents),
                  ASCIIToUTF16(info.contents_class),
                  ASCIIToUTF16(info.description),
                  ASCIIToUTF16(info.description_class),
                  visit_time.ToInternalValue(),
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
  Shortcut shortcut(ShortcutFromTestInfo(shortcut_test_db[1]));
  shortcut.contents = ASCIIToUTF16("gro.todhsals");
  EXPECT_TRUE(db_->UpdateShortcut(shortcut));
  std::map<std::string, Shortcut> shortcuts;
  EXPECT_TRUE(db_->LoadShortcuts(&shortcuts));
  std::map<std::string, Shortcut>::iterator it = shortcuts.find(shortcut.id);
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

  std::map<std::string, Shortcut> shortcuts;
  EXPECT_TRUE(db_->LoadShortcuts(&shortcuts));

  std::map<std::string, Shortcut>::iterator it =
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

  std::map<std::string, Shortcut> shortcuts;
  EXPECT_TRUE(db_->LoadShortcuts(&shortcuts));

  std::map<std::string, Shortcut>::iterator it =
      shortcuts.find(shortcut_test_db[0].guid);
  EXPECT_TRUE(it != shortcuts.end());

  it = shortcuts.find(shortcut_test_db[1].guid);
  EXPECT_TRUE(it == shortcuts.end());

  it = shortcuts.find(shortcut_test_db[2].guid);
  EXPECT_TRUE(it == shortcuts.end());
}

TEST_F(ShortcutsDatabaseTest, LoadShortcuts) {
  AddAll();
  std::map<std::string, Shortcut> shortcuts;
  EXPECT_TRUE(db_->LoadShortcuts(&shortcuts));

  for (size_t i = 0; i < arraysize(shortcut_test_db); ++i) {
    SCOPED_TRACE(base::StringPrintf("Looking for shortcut #%d",
        static_cast<int>(i)));
    EXPECT_TRUE(shortcuts.find(shortcut_test_db[i].guid) != shortcuts.end());
  }
}

TEST_F(ShortcutsDatabaseTest, DeleteAllShortcuts) {
  AddAll();
  std::map<std::string, Shortcut> shortcuts;
  EXPECT_TRUE(db_->LoadShortcuts(&shortcuts));
  EXPECT_EQ(arraysize(shortcut_test_db), shortcuts.size());
  EXPECT_TRUE(db_->DeleteAllShortcuts());
  EXPECT_TRUE(db_->LoadShortcuts(&shortcuts));
  EXPECT_EQ(0U, shortcuts.size());
}

}  // namespace history
