// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_transaction.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/indexed_db_fake_backing_store.h"
#include "content/browser/indexed_db/mock_indexed_db_database_callbacks.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class IndexedDBTransactionTest : public testing::Test {
 public:
  IndexedDBTransactionTest() {
    IndexedDBFactory* factory = NULL;
    backing_store_ = new IndexedDBFakeBackingStore();
    db_ = IndexedDBDatabase::Create(base::ASCIIToUTF16("db"),
                                    backing_store_,
                                    factory,
                                    IndexedDBDatabase::Identifier());
  }

  void RunPostedTasks() { message_loop_.RunUntilIdle(); }
  void DummyOperation(IndexedDBTransaction* transaction) {}

 protected:
  scoped_refptr<IndexedDBFakeBackingStore> backing_store_;
  scoped_refptr<IndexedDBDatabase> db_;

 private:
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBTransactionTest);
};

class IndexedDBTransactionTestMode : public IndexedDBTransactionTest,
  public testing::WithParamInterface<indexed_db::TransactionMode> {
 public:
  IndexedDBTransactionTestMode() {}
 private:
  DISALLOW_COPY_AND_ASSIGN(IndexedDBTransactionTestMode);
};

TEST_F(IndexedDBTransactionTest, Timeout) {
  const int64 id = 0;
  const std::set<int64> scope;
  const bool commit_success = true;
  scoped_refptr<IndexedDBTransaction> transaction = new IndexedDBTransaction(
      id,
      new MockIndexedDBDatabaseCallbacks(),
      scope,
      indexed_db::TRANSACTION_READ_WRITE,
      db_,
      new IndexedDBFakeBackingStore::FakeTransaction(commit_success));
  db_->TransactionCreated(transaction);

  // No conflicting transactions, so coordinator will start it immediately:
  EXPECT_EQ(IndexedDBTransaction::STARTED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());

  // Schedule a task - timer won't be started until it's processed.
  transaction->ScheduleTask(base::Bind(
      &IndexedDBTransactionTest::DummyOperation, base::Unretained(this)));
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());

  RunPostedTasks();
  EXPECT_TRUE(transaction->IsTimeoutTimerRunning());

  // Abort should cancel the timer.
  transaction->Abort();
  EXPECT_EQ(IndexedDBTransaction::FINISHED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());

  // This task will be ignored.
  transaction->ScheduleTask(base::Bind(
      &IndexedDBTransactionTest::DummyOperation, base::Unretained(this)));
  EXPECT_EQ(IndexedDBTransaction::FINISHED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
}

TEST_F(IndexedDBTransactionTest, NoTimeoutReadOnly) {
  const int64 id = 0;
  const std::set<int64> scope;
  const bool commit_success = true;
  scoped_refptr<IndexedDBTransaction> transaction = new IndexedDBTransaction(
      id,
      new MockIndexedDBDatabaseCallbacks(),
      scope,
      indexed_db::TRANSACTION_READ_ONLY,
      db_,
      new IndexedDBFakeBackingStore::FakeTransaction(commit_success));
  db_->TransactionCreated(transaction);

  // No conflicting transactions, so coordinator will start it immediately:
  EXPECT_EQ(IndexedDBTransaction::STARTED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());

  // Schedule a task - timer won't be started until it's processed.
  transaction->ScheduleTask(base::Bind(
      &IndexedDBTransactionTest::DummyOperation, base::Unretained(this)));
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());

  // Transaction is read-only, so no need to time it out.
  RunPostedTasks();
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());

  // Clean up to avoid leaks.
  transaction->Abort();
  EXPECT_EQ(IndexedDBTransaction::FINISHED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
}

class AbortObserver {
 public:
  AbortObserver() : abort_task_called_(false) {}

  void AbortTask(IndexedDBTransaction* transaction) {
    abort_task_called_ = true;
  }

  bool abort_task_called() const { return abort_task_called_; }

 private:
  bool abort_task_called_;
  DISALLOW_COPY_AND_ASSIGN(AbortObserver);
};

TEST_P(IndexedDBTransactionTestMode, AbortTasks) {
  const int64 id = 0;
  const std::set<int64> scope;
  const bool commit_failure = false;
  scoped_refptr<IndexedDBTransaction> transaction = new IndexedDBTransaction(
      id,
      new MockIndexedDBDatabaseCallbacks(),
      scope,
      GetParam(),
      db_,
      new IndexedDBFakeBackingStore::FakeTransaction(commit_failure));
  db_->TransactionCreated(transaction);

  AbortObserver observer;
  transaction->ScheduleTask(
      base::Bind(&IndexedDBTransactionTest::DummyOperation,
                 base::Unretained(this)),
      base::Bind(&AbortObserver::AbortTask, base::Unretained(&observer)));

  // Pump the message loop so that the transaction completes all pending tasks,
  // otherwise it will defer the commit.
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_FALSE(observer.abort_task_called());
  transaction->Commit();
  EXPECT_TRUE(observer.abort_task_called());
  EXPECT_EQ(IndexedDBTransaction::FINISHED, transaction->state());
  EXPECT_FALSE(transaction->IsTimeoutTimerRunning());
}

static const indexed_db::TransactionMode kTestModes[] = {
  indexed_db::TRANSACTION_READ_ONLY,
  indexed_db::TRANSACTION_READ_WRITE,
  indexed_db::TRANSACTION_VERSION_CHANGE
};

INSTANTIATE_TEST_CASE_P(IndexedDBTransactions,
                        IndexedDBTransactionTestMode,
                        ::testing::ValuesIn(kTestModes));

}  // namespace

}  // namespace content
