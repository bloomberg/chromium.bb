// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/strike_database.h"

#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "components/autofill/core/browser/test_strike_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class StrikeDatabaseTest : public ::testing::Test {
 public:
  StrikeDatabaseTest() : db_(InitFilePath()) {}

  void AddEntries(
      std::vector<std::pair<std::string, StrikeData>> entries_to_add) {
    base::RunLoop run_loop;
    db_.AddEntries(
        entries_to_add,
        base::BindRepeating(&StrikeDatabaseTest::OnAddEntries,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void OnAddEntries(base::RepeatingClosure run_loop_closure, bool success) {
    run_loop_closure.Run();
  }

  void OnGetStrikes(base::RepeatingClosure run_loop_closure, int num_strikes) {
    num_strikes_ = num_strikes;
    run_loop_closure.Run();
  }

  int GetStrikes(std::string key) {
    base::RunLoop run_loop;
    db_.GetStrikes(key, base::BindRepeating(&StrikeDatabaseTest::OnGetStrikes,
                                            base::Unretained(this),
                                            run_loop.QuitClosure()));
    run_loop.Run();
    return num_strikes_;
  }

  void OnAddStrike(base::RepeatingClosure run_loop_closure, int num_strikes) {
    num_strikes_ = num_strikes;
    run_loop_closure.Run();
  }

  int AddStrike(std::string key) {
    base::RunLoop run_loop;
    db_.AddStrike(key, base::BindRepeating(&StrikeDatabaseTest::OnAddStrike,
                                           base::Unretained(this),
                                           run_loop.QuitClosure()));
    run_loop.Run();
    return num_strikes_;
  }

  void OnClearAllStrikesForKey(base::RepeatingClosure run_loop_closure,
                               bool success) {
    run_loop_closure.Run();
  }

  void ClearAllStrikesForKey(const std::string key) {
    base::RunLoop run_loop;
    db_.ClearAllStrikesForKey(
        key,
        base::BindRepeating(&StrikeDatabaseTest::OnClearAllStrikesForKey,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  TestStrikeDatabase db_;

 private:
  static const base::FilePath InitFilePath() {
    base::ScopedTempDir temp_dir_;
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    const base::FilePath file_path =
        temp_dir_.GetPath().AppendASCII("StrikeDatabaseTest");
    return file_path;
  }

  int num_strikes_;
  std::unique_ptr<StrikeData> strike_data_;
};

TEST_F(StrikeDatabaseTest, AddStrikeTest) {
  const std::string key = "12345";
  int strikes = AddStrike(key);
  EXPECT_EQ(1, strikes);
  strikes = AddStrike(key);
  EXPECT_EQ(2, strikes);
}

TEST_F(StrikeDatabaseTest, GetStrikeForZeroStrikesTest) {
  const std::string key = "12345";
  int strikes = GetStrikes(key);
  EXPECT_EQ(0, strikes);
}

TEST_F(StrikeDatabaseTest, GetStrikeForNonZeroStrikesTest) {
  // Set up database with 3 pre-existing strikes at |key|.
  const std::string key = "12345";
  std::vector<std::pair<std::string, StrikeData>> entries;
  StrikeData data;
  data.set_num_strikes(3);
  entries.push_back(std::make_pair(key, data));
  AddEntries(entries);

  int strikes = GetStrikes(key);
  EXPECT_EQ(3, strikes);
}

TEST_F(StrikeDatabaseTest, ClearStrikesForZeroStrikesTest) {
  const std::string key = "12345";
  ClearAllStrikesForKey(key);
  int strikes = GetStrikes(key);
  EXPECT_EQ(0, strikes);
}

TEST_F(StrikeDatabaseTest, ClearStrikesForNonZeroStrikesTest) {
  // Set up database with 3 pre-existing strikes at |key|.
  const std::string key = "12345";
  std::vector<std::pair<std::string, StrikeData>> entries;
  StrikeData data;
  data.set_num_strikes(3);
  entries.push_back(std::make_pair(key, data));
  AddEntries(entries);

  int strikes = GetStrikes(key);
  EXPECT_EQ(3, strikes);
  ClearAllStrikesForKey(key);
  strikes = GetStrikes(key);
  EXPECT_EQ(0, strikes);
}

TEST_F(StrikeDatabaseTest, ClearStrikesForMultipleNonZeroStrikesEntriesTest) {
  // Set up database with 3 pre-existing strikes at |key1|, and 5 pre-existing
  // strikes at |key2|.
  const std::string key1 = "12345";
  const std::string key2 = "13579";
  std::vector<std::pair<std::string, StrikeData>> entries;
  StrikeData data1;
  data1.set_num_strikes(3);
  entries.push_back(std::make_pair(key1, data1));
  StrikeData data2;
  data2.set_num_strikes(5);
  entries.push_back(std::make_pair(key2, data2));
  AddEntries(entries);

  int strikes = GetStrikes(key1);
  EXPECT_EQ(3, strikes);
  strikes = GetStrikes(key2);
  EXPECT_EQ(5, strikes);
  ClearAllStrikesForKey(key1);
  strikes = GetStrikes(key1);
  EXPECT_EQ(0, strikes);
  strikes = GetStrikes(key2);
  EXPECT_EQ(5, strikes);
}

TEST_F(StrikeDatabaseTest, GetKeyForCreditCardSave) {
  const std::string last_four = "1234";
  EXPECT_EQ("creditCardSave__1234", db_.GetKeyForCreditCardSave(last_four));
}

}  // namespace autofill
