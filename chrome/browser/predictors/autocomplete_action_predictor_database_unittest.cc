// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/message_loop.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_database.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "sql/statement.h"

#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;
using content::BrowserThread;

namespace {

struct AutocompleteActionPredictorDatabase::Row test_db[] = {
  AutocompleteActionPredictorDatabase::Row(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880DF",
      ASCIIToUTF16("goog"), GURL("http://www.google.com/"),
      1, 0),
  AutocompleteActionPredictorDatabase::Row(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880E0",
      ASCIIToUTF16("slash"), GURL("http://slashdot.org/"),
      3, 2),
  AutocompleteActionPredictorDatabase::Row(
      "BD85DBA2-8C29-49F9-84AE-48E1E90880E1",
      ASCIIToUTF16("news"), GURL("http://slashdot.org/"),
      0, 1),
};

}  // end namespace

class AutocompleteActionPredictorDatabaseTest : public testing::Test {
 public:
  AutocompleteActionPredictorDatabaseTest();
  virtual ~AutocompleteActionPredictorDatabaseTest();

  virtual void SetUp();
  virtual void TearDown();

  size_t CountRecords() const;

  void AddAll();

  bool RowsAreEqual(const AutocompleteActionPredictorDatabase::Row& lhs,
                    const AutocompleteActionPredictorDatabase::Row& rhs) const;

  TestingProfile* profile() { return &profile_; }

 protected:

  // Test functions that can be run against this text fixture or
  // AutocompleteActionPredictorDatabaseReopenTest that inherits from this.
  void TestAddRow();
  void TestGetRow();
  void TestUpdateRow();
  void TestDeleteRow();
  void TestDeleteRows();
  void TestDeleteAllRows();

 private:
  TestingProfile profile_;
  scoped_refptr<AutocompleteActionPredictorDatabase> db_;
  MessageLoop loop_;
  content::TestBrowserThread db_thread_;
};

class AutocompleteActionPredictorDatabaseReopenTest
    : public AutocompleteActionPredictorDatabaseTest {
 public:
  virtual void SetUp() {
    // By calling SetUp twice, we make sure that the table already exists for
    // this fixture.
    AutocompleteActionPredictorDatabaseTest::SetUp();
    AutocompleteActionPredictorDatabaseTest::TearDown();
    AutocompleteActionPredictorDatabaseTest::SetUp();
  }
};

AutocompleteActionPredictorDatabaseTest::
AutocompleteActionPredictorDatabaseTest()
    : loop_(MessageLoop::TYPE_DEFAULT),
      db_thread_(BrowserThread::DB, &loop_) {
}

AutocompleteActionPredictorDatabaseTest::
~AutocompleteActionPredictorDatabaseTest() {
}

void AutocompleteActionPredictorDatabaseTest::SetUp() {
  db_ = new AutocompleteActionPredictorDatabase(&profile_);
  db_->Initialize();
}

void AutocompleteActionPredictorDatabaseTest::TearDown() {
  db_ = NULL;
}

size_t AutocompleteActionPredictorDatabaseTest::CountRecords() const {
  sql::Statement s(db_->db_.GetUniqueStatement(
      "SELECT count(*) FROM network_action_predictor"));
  EXPECT_TRUE(s.Step());
  return static_cast<size_t>(s.ColumnInt(0));
}

void AutocompleteActionPredictorDatabaseTest::AddAll() {
  for (size_t i = 0; i < arraysize(test_db); ++i)
    db_->AddRow(test_db[i]);

  EXPECT_EQ(arraysize(test_db), CountRecords());
}

bool AutocompleteActionPredictorDatabaseTest::RowsAreEqual(
    const AutocompleteActionPredictorDatabase::Row& lhs,
    const AutocompleteActionPredictorDatabase::Row& rhs) const {
  return (lhs.id == rhs.id &&
          lhs.user_text == rhs.user_text &&
          lhs.url == rhs.url &&
          lhs.number_of_hits == rhs.number_of_hits &&
          lhs.number_of_misses == rhs.number_of_misses);
}

void AutocompleteActionPredictorDatabaseTest::TestAddRow() {
  EXPECT_EQ(0U, CountRecords());
  db_->AddRow(test_db[0]);
  EXPECT_EQ(1U, CountRecords());
  db_->AddRow(test_db[1]);
  EXPECT_EQ(2U, CountRecords());
  db_->AddRow(test_db[2]);
  EXPECT_EQ(3U, CountRecords());
}

void AutocompleteActionPredictorDatabaseTest::TestGetRow() {
  db_->AddRow(test_db[0]);
  AutocompleteActionPredictorDatabase::Row row;
  db_->GetRow(test_db[0].id, &row);
  EXPECT_TRUE(RowsAreEqual(test_db[0], row))
      << "Expected: Row with id " << test_db[0].id << "\n"
      << "Got:      Row with id " << row.id;
}

void AutocompleteActionPredictorDatabaseTest::TestUpdateRow() {
  AddAll();
  AutocompleteActionPredictorDatabase::Row row = test_db[1];
  row.number_of_hits = row.number_of_hits + 1;
  db_->UpdateRow(row);

  AutocompleteActionPredictorDatabase::Row updated_row;
  db_->GetRow(test_db[1].id, &updated_row);

  EXPECT_TRUE(RowsAreEqual(row, updated_row))
      << "Expected: Row with id " << row.id << "\n"
      << "Got:      Row with id " << updated_row.id;
}

void AutocompleteActionPredictorDatabaseTest::TestDeleteRow() {
  AddAll();
  db_->DeleteRow(test_db[2].id);
  EXPECT_EQ(arraysize(test_db) - 1, CountRecords());
}

void AutocompleteActionPredictorDatabaseTest::TestDeleteRows() {
  AddAll();
  std::vector<AutocompleteActionPredictorDatabase::Row::Id> id_list;
  id_list.push_back(test_db[0].id);
  id_list.push_back(test_db[2].id);
  db_->DeleteRows(id_list);
  EXPECT_EQ(arraysize(test_db) - 2, CountRecords());

  AutocompleteActionPredictorDatabase::Row row;
  db_->GetRow(test_db[1].id, &row);
  EXPECT_TRUE(RowsAreEqual(test_db[1], row));
}

void AutocompleteActionPredictorDatabaseTest::TestDeleteAllRows() {
  AddAll();
  db_->DeleteAllRows();
  EXPECT_EQ(0U, CountRecords());
}

// AutocompleteActionPredictorDatabaseTest tests
TEST_F(AutocompleteActionPredictorDatabaseTest, AddRow) {
  TestAddRow();
}

TEST_F(AutocompleteActionPredictorDatabaseTest, GetRow) {
  TestGetRow();
}

TEST_F(AutocompleteActionPredictorDatabaseTest, UpdateRow) {
  TestUpdateRow();
}

TEST_F(AutocompleteActionPredictorDatabaseTest, DeleteRow) {
  TestDeleteRow();
}

TEST_F(AutocompleteActionPredictorDatabaseTest, DeleteRows) {
  TestDeleteRows();
}

TEST_F(AutocompleteActionPredictorDatabaseTest, DeleteAllRows) {
  TestDeleteAllRows();
}

// AutocompleteActionPredictorDatabaseReopenTest tests
TEST_F(AutocompleteActionPredictorDatabaseReopenTest, AddRow) {
  TestAddRow();
}

TEST_F(AutocompleteActionPredictorDatabaseReopenTest, GetRow) {
  TestGetRow();
}

TEST_F(AutocompleteActionPredictorDatabaseReopenTest, UpdateRow) {
  TestUpdateRow();
}

TEST_F(AutocompleteActionPredictorDatabaseReopenTest, DeleteRow) {
  TestDeleteRow();
}

TEST_F(AutocompleteActionPredictorDatabaseReopenTest, DeleteRows) {
  TestDeleteRows();
}

TEST_F(AutocompleteActionPredictorDatabaseReopenTest, DeleteAllRows) {
  TestDeleteAllRows();
}
