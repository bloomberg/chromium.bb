// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/store/prefetch_store.h"

#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {

int CountPrefetchItems(sql::Connection* db) {
  // Not starting transaction as this is a single read.
  static const char kSql[] = "SELECT COUNT(offline_id) FROM prefetch_items";
  sql::Statement statement(db->GetUniqueStatement(kSql));
  if (statement.Step())
    return statement.ColumnInt(0);

  return -1;
}

void CountPrefetchItemsResult(int expected_count, int actual_count) {
  EXPECT_EQ(expected_count, actual_count);
}

}  // namespace

class PrefetchStoreTest : public testing::Test {
 public:
  PrefetchStoreTest();
  ~PrefetchStoreTest() override = default;

  void SetUp() override { store_test_util_.BuildStoreInMemory(); }

  void TearDown() override {
    store_test_util_.DeleteStore();
    PumpLoop();
  }

  PrefetchStore* store() { return store_test_util_.store(); }

  void PumpLoop() { task_runner_->RunUntilIdle(); }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  PrefetchStoreTestUtil store_test_util_;
};

PrefetchStoreTest::PrefetchStoreTest()
    : task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_),
      store_test_util_(task_runner_) {}

TEST_F(PrefetchStoreTest, InitializeStore) {
  store()->Execute<int>(base::BindOnce(&CountPrefetchItems),
                        base::BindOnce(&CountPrefetchItemsResult, 0));
  PumpLoop();
}

}  // namespace offline_pages
