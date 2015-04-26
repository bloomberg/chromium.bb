// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/statistics_table.h"

#include "base/files/scoped_temp_dir.h"
#include "sql/connection.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

const char kTestDomain[] = "http://google.com";

void CheckStatsAreEqual(const InteractionsStats& left,
                        const InteractionsStats& right) {
  EXPECT_EQ(left.origin_domain, right.origin_domain);
  EXPECT_EQ(left.nopes_count, right.nopes_count);
  EXPECT_EQ(left.dismissal_count, right.dismissal_count);
  EXPECT_EQ(left.start_date, right.start_date);
}

class StatisticsTableTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ReloadDatabase();

    test_data_.origin_domain = GURL(kTestDomain);
    test_data_.nopes_count = 5;
    test_data_.dismissal_count = 10;
    test_data_.start_date = base::Time::FromTimeT(1);
  }

  void ReloadDatabase() {
    base::FilePath file = temp_dir_.path().AppendASCII("TestDatabase");
    db_.reset(new StatisticsTable);
    connection_.reset(new sql::Connection);
    connection_->set_exclusive_locking();
    ASSERT_TRUE(connection_->Open(file));
    ASSERT_TRUE(db_->Init(connection_.get()));
  }

  InteractionsStats& test_data() { return test_data_; }
  StatisticsTable* db() { return db_.get(); }

 private:
  base::ScopedTempDir temp_dir_;
  scoped_ptr<sql::Connection> connection_;
  scoped_ptr<StatisticsTable> db_;
  InteractionsStats test_data_;
};

TEST_F(StatisticsTableTest, Sanity) {
  EXPECT_TRUE(db()->AddRow(test_data()));
  scoped_ptr<InteractionsStats> stats = db()->GetRow(test_data().origin_domain);
  ASSERT_TRUE(stats);
  CheckStatsAreEqual(test_data(), *stats);
  EXPECT_TRUE(db()->RemoveRow(test_data().origin_domain));
  EXPECT_FALSE(db()->GetRow(test_data().origin_domain));
}

TEST_F(StatisticsTableTest, Reload) {
  EXPECT_TRUE(db()->AddRow(test_data()));
  EXPECT_TRUE(db()->GetRow(test_data().origin_domain));

  ReloadDatabase();

  scoped_ptr<InteractionsStats> stats = db()->GetRow(test_data().origin_domain);
  ASSERT_TRUE(stats);
  CheckStatsAreEqual(test_data(), *stats);
}

TEST_F(StatisticsTableTest, DoubleOperation) {
  EXPECT_TRUE(db()->AddRow(test_data()));
  test_data().nopes_count++;
  EXPECT_TRUE(db()->AddRow(test_data()));

  scoped_ptr<InteractionsStats> stats = db()->GetRow(test_data().origin_domain);
  ASSERT_TRUE(stats);
  CheckStatsAreEqual(test_data(), *stats);

  EXPECT_TRUE(db()->RemoveRow(test_data().origin_domain));
  EXPECT_FALSE(db()->GetRow(test_data().origin_domain));
  EXPECT_TRUE(db()->RemoveRow(test_data().origin_domain));
  EXPECT_FALSE(db()->GetRow(test_data().origin_domain));
}

}  // namespace
}  // namespace password_manager
