// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/safe_browsing/db/whitelist_checker_client.h"

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/safe_browsing/db/test_database_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

using base::TimeDelta;
using testing::_;
using testing::Return;
using testing::SaveArg;

using BoolCallback = base::Callback<void(bool /* is_whitelisted */)>;
using MockBoolCallback = testing::StrictMock<base::MockCallback<BoolCallback>>;

namespace {
class MockSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager() {}

  MOCK_METHOD1(CancelCheck, void(SafeBrowsingDatabaseManager::Client*));

  MOCK_METHOD2(CheckCsdWhitelistUrl,
               AsyncMatch(const GURL&, SafeBrowsingDatabaseManager::Client*));

 protected:
  ~MockSafeBrowsingDatabaseManager() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingDatabaseManager);
};
}  // namespace

class WhitelistCheckerClientTest : public testing::Test {
 public:
  WhitelistCheckerClientTest()
      : thread_bundle_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME),
        target_url_("http://foo.bar") {}

  void SetUp() override {
    database_manager_ = new MockSafeBrowsingDatabaseManager;
  }

  void TearDown() override {
    database_manager_ = nullptr;
    base::RunLoop().RunUntilIdle();

    // Verify no callback is remaining.
    // TODO(nparker): We should somehow EXPECT that no entry is remaining,
    // rather than just invoking it.
    thread_bundle_.FastForwardUntilNoTasksRemain();
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;

  GURL target_url_;
  scoped_refptr<MockSafeBrowsingDatabaseManager> database_manager_;
};

TEST_F(WhitelistCheckerClientTest, TestMatch) {
  EXPECT_CALL(*database_manager_, CheckCsdWhitelistUrl(target_url_, _))
      .WillOnce(Return(AsyncMatch::MATCH));

  MockBoolCallback callback;
  EXPECT_CALL(callback, Run(true /* is_whitelisted */));
  WhitelistCheckerClient::StartCheckCsdWhitelist(database_manager_, target_url_,
                                                 callback.Get());
}

TEST_F(WhitelistCheckerClientTest, TestNoMatch) {
  EXPECT_CALL(*database_manager_, CheckCsdWhitelistUrl(target_url_, _))
      .WillOnce(Return(AsyncMatch::NO_MATCH));

  MockBoolCallback callback;
  EXPECT_CALL(callback, Run(false /* is_whitelisted */));
  WhitelistCheckerClient::StartCheckCsdWhitelist(database_manager_, target_url_,
                                                 callback.Get());
}

TEST_F(WhitelistCheckerClientTest, TestAsyncNoMatch) {
  SafeBrowsingDatabaseManager::Client* client;
  EXPECT_CALL(*database_manager_, CheckCsdWhitelistUrl(target_url_, _))
      .WillOnce(DoAll(SaveArg<1>(&client), Return(AsyncMatch::ASYNC)));

  MockBoolCallback callback;
  WhitelistCheckerClient::StartCheckCsdWhitelist(database_manager_, target_url_,
                                                 callback.Get());
  // Callback should not be called yet.

  EXPECT_CALL(callback, Run(false /* is_whitelisted */));
  // The self-owned client deletes itself here.
  client->OnCheckWhitelistUrlResult(false);
}

TEST_F(WhitelistCheckerClientTest, TestAsyncTimeout) {
  SafeBrowsingDatabaseManager::Client* client;
  EXPECT_CALL(*database_manager_, CheckCsdWhitelistUrl(target_url_, _))
      .WillOnce(DoAll(SaveArg<1>(&client), Return(AsyncMatch::ASYNC)));
  EXPECT_CALL(*database_manager_, CancelCheck(_)).Times(1);

  MockBoolCallback callback;
  WhitelistCheckerClient::StartCheckCsdWhitelist(database_manager_, target_url_,
                                                 callback.Get());
  thread_bundle_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  // No callback yet.

  EXPECT_CALL(callback, Run(true /* is_whitelisted */));
  thread_bundle_.FastForwardBy(base::TimeDelta::FromSeconds(5));
}

}  // namespace safe_browsing
