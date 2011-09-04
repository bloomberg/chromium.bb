// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/url_blacklist_manager.h"

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;

class TestingURLBlacklistManager : public URLBlacklistManager {
 public:
  explicit TestingURLBlacklistManager(PrefService* pref_service)
      : URLBlacklistManager(pref_service) {
  }

  virtual ~TestingURLBlacklistManager() {
  }

  // Make this method public for testing.
  using URLBlacklistManager::ScheduleUpdate;

  // Post tasks without a delay during tests.
  virtual void PostUpdateTask(Task* task) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE, task);
  }

  // Makes a direct call to UpdateOnIO during tests.
  void UpdateOnIO() {
    StringVector* block = new StringVector;
    block->push_back("example.com");
    StringVector* allow = new StringVector;
    URLBlacklistManager::UpdateOnIO(block, allow);
  }

  void UpdateNotMocked() {
    URLBlacklistManager::Update();
  }

  MOCK_METHOD0(Update, void());
  MOCK_METHOD1(SetBlacklist, void(URLBlacklist*));

 private:
  DISALLOW_COPY_AND_ASSIGN(TestingURLBlacklistManager);
};

class URLBlacklistManagerTest : public testing::Test {
 protected:
  URLBlacklistManagerTest()
      : ui_thread_(BrowserThread::UI, &loop_),
        file_thread_(BrowserThread::FILE, &loop_),
        io_thread_(BrowserThread::IO, &loop_) {
  }

  virtual void SetUp() OVERRIDE {
    pref_service_.RegisterListPref(prefs::kUrlBlacklist);
    pref_service_.RegisterListPref(prefs::kUrlWhitelist);
    blacklist_manager_.reset(
        new TestingURLBlacklistManager(&pref_service_));
    loop_.RunAllPending();
  }

  virtual void TearDown() OVERRIDE {
    if (blacklist_manager_.get())
      blacklist_manager_->ShutdownOnUIThread();
    loop_.RunAllPending();
    // Delete |blacklist_manager_| while |io_thread_| is mapping IO to
    // |loop_|.
    blacklist_manager_.reset();
  }

  void ExpectUpdate() {
    EXPECT_CALL(*blacklist_manager_, Update())
        .WillOnce(Invoke(blacklist_manager_.get(),
                         &TestingURLBlacklistManager::UpdateNotMocked));
  }

  MessageLoop loop_;
  TestingPrefService pref_service_;
  scoped_ptr<TestingURLBlacklistManager> blacklist_manager_;

 private:
  BrowserThread ui_thread_;
  BrowserThread file_thread_;
  BrowserThread io_thread_;

  DISALLOW_COPY_AND_ASSIGN(URLBlacklistManagerTest);
};

TEST_F(URLBlacklistManagerTest, SingleUpdateForTwoPrefChanges) {
  ExpectUpdate();

  ListValue* blacklist = new ListValue;
  blacklist->Append(new StringValue("*.google.com"));
  ListValue* whitelist = new ListValue;
  whitelist->Append(new StringValue("mail.google.com"));
  pref_service_.SetManagedPref(prefs::kUrlBlacklist, blacklist);
  pref_service_.SetManagedPref(prefs::kUrlBlacklist, whitelist);
  loop_.RunAllPending();

  Mock::VerifyAndClearExpectations(blacklist_manager_.get());
}

TEST_F(URLBlacklistManagerTest, ShutdownWithPendingTask0) {
  // Post an update task to the UI thread.
  blacklist_manager_->ScheduleUpdate();
  // Shutdown comes before the task is executed.
  blacklist_manager_->ShutdownOnUIThread();
  blacklist_manager_.reset();
  // Run the task after shutdown and deletion.
  loop_.RunAllPending();
}

TEST_F(URLBlacklistManagerTest, ShutdownWithPendingTask1) {
  EXPECT_CALL(*blacklist_manager_, Update()).Times(0);
  // Post an update task.
  blacklist_manager_->ScheduleUpdate();
  // Shutdown comes before the task is executed.
  blacklist_manager_->ShutdownOnUIThread();
  // Run the task after shutdown, but before deletion.
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(blacklist_manager_.get());
  blacklist_manager_.reset();
  loop_.RunAllPending();
}

TEST_F(URLBlacklistManagerTest, ShutdownWithPendingTask2) {
  // Update posts a BuildBlacklistTask to the FILE thread.
  blacklist_manager_->UpdateNotMocked();
  // Shutdown comes before the task is executed.
  blacklist_manager_->ShutdownOnUIThread();
  blacklist_manager_.reset();
  // Run the task after shutdown and deletion.
  loop_.RunAllPending();
}

TEST_F(URLBlacklistManagerTest, ShutdownWithPendingTask3) {
  EXPECT_CALL(*blacklist_manager_, SetBlacklist(_)).Times(0);
  // Update posts a BuildBlacklistTask to the FILE thread.
  blacklist_manager_->UpdateNotMocked();
  // Shutdown comes before the task is executed.
  blacklist_manager_->ShutdownOnUIThread();
  // Run the task after shutdown, but before deletion.
  loop_.RunAllPending();
  Mock::VerifyAndClearExpectations(blacklist_manager_.get());
  blacklist_manager_.reset();
  loop_.RunAllPending();
}

TEST_F(URLBlacklistManagerTest, ShutdownWithPendingTask4) {
  EXPECT_CALL(*blacklist_manager_, SetBlacklist(_)).Times(0);

  // This posts a task to the FILE thread.
  blacklist_manager_->UpdateOnIO();
  // But shutdown happens before it is done.
  blacklist_manager_->ShutdownOnUIThread();
  blacklist_manager_.reset();
  loop_.RunAllPending();

  Mock::VerifyAndClearExpectations(blacklist_manager_.get());
}

TEST_F(URLBlacklistManagerTest, ShutdownWithPendingTask5) {
  EXPECT_CALL(*blacklist_manager_, SetBlacklist(_)).Times(0);

  // This posts a task to the FILE thread.
  blacklist_manager_->UpdateOnIO();
  // But shutdown happens before it is done.
  blacklist_manager_->ShutdownOnUIThread();
  // This time, shutdown on UI is done but the object is still alive.
  loop_.RunAllPending();
  blacklist_manager_.reset();
  loop_.RunAllPending();

  Mock::VerifyAndClearExpectations(blacklist_manager_.get());
}

}  // namespace

}  // namespace policy
