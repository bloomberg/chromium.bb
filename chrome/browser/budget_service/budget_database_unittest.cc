// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/budget_service/budget_database.h"

#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/budget_service/budget.pb.h"
#include "chrome/test/base/testing_profile.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/proto_database_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const double kDefaultBudget1 = 1.234;
const double kDefaultBudget2 = 2.345;
const double kDefaultExpirationInHours = 72;

const char kTestOrigin[] = "https://example.com";

}  // namespace

class BudgetDatabaseTest : public ::testing::Test {
 public:
  BudgetDatabaseTest()
      : success_(false),
        db_(profile_.GetPath().Append(FILE_PATH_LITERAL("BudgetDabase")),
            base::ThreadTaskRunnerHandle::Get()) {}

  // The BudgetDatabase assumes that a budget will always be queried before it
  // is written to. Use GetBudgetDetails() to pre-populate the cache.
  void SetUp() override { GetBudgetDetails(); }

  void AddBudgetComplete(base::Closure run_loop_closure, bool success) {
    success_ = success;
    run_loop_closure.Run();
  }

  // Add budget to the origin.
  bool AddBudget(const GURL& origin, double amount) {
    base::RunLoop run_loop;
    db_.AddBudget(origin, amount,
                  base::Bind(&BudgetDatabaseTest::AddBudgetComplete,
                             base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return success_;
  }

  void GetBudgetDetailsComplete(
      base::Closure run_loop_closure,
      bool success,
      double debt,
      const BudgetDatabase::BudgetExpectation& expectation) {
    success_ = success;
    debt_ = debt;
    expectation_ = expectation;
    run_loop_closure.Run();
  }

  // Get the full set of budget expectations for the origin.
  void GetBudgetDetails() {
    base::RunLoop run_loop;
    db_.GetBudgetDetails(
        GURL(kTestOrigin),
        base::Bind(&BudgetDatabaseTest::GetBudgetDetailsComplete,
                   base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  Profile* profile() { return &profile_; }
  const BudgetDatabase::BudgetExpectation& expectation() {
    return expectation_;
  }

  // Setup a test clock so that the tests can control time.
  base::SimpleTestClock* SetClockForTesting() {
    base::SimpleTestClock* clock = new base::SimpleTestClock();
    db_.SetClockForTesting(base::WrapUnique(clock));
    return clock;
  }

  // Query the database to check if the origin is in the cache.
  bool IsCached(const GURL& origin) { return db_.IsCached(origin); }

 protected:
  bool success_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<budget_service::Budget> budget_;
  TestingProfile profile_;
  BudgetDatabase db_;
  double debt_ = 0;
  BudgetDatabase::BudgetExpectation expectation_;
};

TEST_F(BudgetDatabaseTest, ReadAndWriteTest) {
  const GURL origin(kTestOrigin);
  base::SimpleTestClock* clock = SetClockForTesting();
  base::TimeDelta expiration(
      base::TimeDelta::FromHours(kDefaultExpirationInHours));
  base::Time starting_time = clock->Now();
  base::Time expiration_time = clock->Now() + expiration;

  // Add two budget chunks with different expirations (default expiration and
  // default expiration + 1 day).
  ASSERT_TRUE(AddBudget(origin, kDefaultBudget1));
  clock->Advance(base::TimeDelta::FromDays(1));
  ASSERT_TRUE(AddBudget(origin, kDefaultBudget2));

  // Get the budget.
  GetBudgetDetails();

  // Get the expectation and validate it.
  const auto& expected_value = expectation();
  ASSERT_TRUE(success_);
  ASSERT_EQ(3U, expected_value.size());

  // Make sure that the correct data is returned.
  auto iter = expected_value.begin();

  // First value should be [total_budget, now]
  EXPECT_EQ(kDefaultBudget1 + kDefaultBudget2, iter->first);
  EXPECT_EQ(clock->Now(), iter->second);

  // The next value should be the budget after the first chunk expires.
  iter++;
  EXPECT_EQ(kDefaultBudget2, iter->first);
  EXPECT_EQ(expiration_time, iter->second);

  // The final value gives the budget of 0.0 after the second chunk expires.
  expiration_time += base::TimeDelta::FromDays(1);
  iter++;
  EXPECT_EQ(0, iter->first);
  EXPECT_EQ(expiration_time, iter->second);

  // Advance the time until the first chunk of budget should be expired.
  clock->SetNow(starting_time +
                base::TimeDelta::FromHours(kDefaultExpirationInHours));

  // Get the new budget and check that kDefaultBudget1 has been removed.
  GetBudgetDetails();
  iter = expectation().begin();
  ASSERT_EQ(2U, expectation().size());
  EXPECT_EQ(kDefaultBudget2, iter->first);
  iter++;
  EXPECT_EQ(0, iter->first);

  // Advace the time until both chunks of budget should be expired.
  clock->SetNow(starting_time +
                base::TimeDelta::FromHours(kDefaultExpirationInHours) +
                base::TimeDelta::FromDays(1));

  GetBudgetDetails();
  iter = expectation().begin();
  ASSERT_EQ(1U, expectation().size());
  EXPECT_EQ(0, iter->first);

  // Now that the entire budget has expired, check that the entry in the map
  // has been removed.
  EXPECT_FALSE(IsCached(origin));
}
