// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/budget_service/budget_database.h"

#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/budget_service/budget.pb.h"
#include "chrome/test/base/testing_profile.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/proto_database_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const double kDefaultDebt = -0.72;
const double kDefaultBudget1 = 1.234;
const double kDefaultBudget2 = 2.345;
const double kDefaultExpiration = 3600000000;

const char kTestOrigin[] = "https://example.com";

class BudgetDatabaseTest : public ::testing::Test {
 public:
  BudgetDatabaseTest()
      : success_(false),
        db_(profile_.GetPath().Append(FILE_PATH_LITERAL("BudgetDabase")),
            base::ThreadTaskRunnerHandle::Get()) {}

  void SetBudgetComplete(base::Closure run_loop_closure, bool success) {
    success_ = success;
    run_loop_closure.Run();
  }

  // Set up basic default values.
  bool SetBudgetWithDefaultValues() {
    budget_service::Budget budget;
    budget.set_debt(kDefaultDebt);

    // Create two chunks and give them default values.
    budget_service::BudgetChunk* budget_chunk = budget.add_budget();
    budget_chunk->set_budget_amount(kDefaultBudget1);
    budget_chunk->set_expiration_timestamp(kDefaultExpiration);
    budget_chunk = budget.add_budget();
    budget_chunk->set_budget_amount(kDefaultBudget2);
    budget_chunk->set_expiration_timestamp(kDefaultExpiration + 1);

    base::RunLoop run_loop;
    db_.SetValue(GURL(kTestOrigin), budget,
                 base::Bind(&BudgetDatabaseTest::SetBudgetComplete,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();

    return success_;
  }

  void GetBudgetComplete(base::Closure run_loop_closure,
                         bool success,
                         std::unique_ptr<budget_service::Budget> budget) {
    budget_ = std::move(budget);
    success_ = success;
    run_loop_closure.Run();
  }

  // Excercise the very basic get method.
  std::unique_ptr<budget_service::Budget> GetBudget() {
    base::RunLoop run_loop;
    db_.GetValue(GURL(kTestOrigin),
                 base::Bind(&BudgetDatabaseTest::GetBudgetComplete,
                            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return std::move(budget_);
  }

  Profile* profile() { return &profile_; }

 protected:
  bool success_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<budget_service::Budget> budget_;
  TestingProfile profile_;
  BudgetDatabase db_;
};

TEST_F(BudgetDatabaseTest, ReadAndWriteTest) {
  ASSERT_TRUE(SetBudgetWithDefaultValues());
  std::unique_ptr<budget_service::Budget> b = GetBudget();

  ASSERT_TRUE(success_);
  EXPECT_EQ(kDefaultDebt, b->debt());
  EXPECT_EQ(kDefaultBudget1, b->budget(0).budget_amount());
  EXPECT_EQ(kDefaultBudget2, b->budget(1).budget_amount());
  EXPECT_EQ(kDefaultExpiration, b->budget(0).expiration_timestamp());
}

}  // namespace
