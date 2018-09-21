// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/strike_database.h"

#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/autofill/strike_data.pb.h"
#include "chrome/browser/autofill/test_strike_database.h"
#include "chrome/test/base/testing_profile.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/proto_database_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class StrikeDatabaseTest : public ::testing::Test {
 public:
  StrikeDatabaseTest()
      : db_(profile_.GetPath().Append(FILE_PATH_LITERAL("StrikeDatabase"))) {}

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

 private:
  int num_strikes_;
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<StrikeData> strike_data_;
  TestingProfile profile_;
  TestStrikeDatabase db_;
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

}  // namespace autofill
