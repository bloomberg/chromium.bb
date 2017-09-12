// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/media/router/issue_manager.h"
#include "chrome/browser/media/router/mock_media_router.h"
#include "chrome/browser/media/router/test_helper.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::SaveArg;

namespace media_router {
namespace {

IssueInfo CreateTestIssue(IssueInfo::Severity severity) {
  IssueInfo issue("title", IssueInfo::Action::DISMISS, severity);
  issue.message = "message";
  issue.help_page_id = 12345;
  return issue;
}

}  // namespace

class IssueManagerTest : public ::testing::Test {
 protected:
  IssueManagerTest()
      : task_runner_(new base::TestMockTimeTaskRunner()),
        runner_handler_(task_runner_) {
    manager_.set_task_runner_for_test(task_runner_);
  }
  ~IssueManagerTest() override {}

  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle runner_handler_;
  IssueManager manager_;
  MockMediaRouter router_;
};

TEST_F(IssueManagerTest, AddAndClearIssue) {
  IssueInfo issue_info1 = CreateTestIssue(IssueInfo::Severity::WARNING);

  // Add initial issue.
  manager_.AddIssue(issue_info1);

  Issue issue1((IssueInfo()));
  MockIssuesObserver observer(&router_);
  EXPECT_CALL(observer, OnIssue(_)).WillOnce(SaveArg<0>(&issue1));
  manager_.RegisterObserver(&observer);
  EXPECT_EQ(issue_info1, issue1.info());
  Issue::Id issue1_id = issue1.id();

  IssueInfo issue_info2 = CreateTestIssue(IssueInfo::Severity::FATAL);
  EXPECT_TRUE(issue_info2.is_blocking);

  // Blocking issue takes precedence.
  Issue issue2((IssueInfo()));
  EXPECT_CALL(observer, OnIssue(_)).WillOnce(SaveArg<0>(&issue2));
  manager_.AddIssue(issue_info2);
  EXPECT_EQ(issue_info2, issue2.info());

  // Clear |issue2|. Observer will be notified with |issue1| again as it is now
  // the top issue.
  EXPECT_CALL(observer, OnIssue(_)).WillOnce(SaveArg<0>(&issue1));
  manager_.ClearIssue(issue2.id());
  EXPECT_EQ(issue1_id, issue1.id());
  EXPECT_EQ(issue_info1, issue1.info());

  // All issues cleared. Observer will be notified with |nullptr| that there are
  // no more issues.
  EXPECT_CALL(observer, OnIssuesCleared());
  manager_.ClearIssue(issue1.id());

  manager_.UnregisterObserver(&observer);
}

TEST_F(IssueManagerTest, AddSameIssueInfoHasNoEffect) {
  IssueInfo issue_info = CreateTestIssue(IssueInfo::Severity::WARNING);

  MockIssuesObserver observer(&router_);
  manager_.RegisterObserver(&observer);

  Issue issue((IssueInfo()));
  EXPECT_CALL(observer, OnIssue(_)).WillOnce(SaveArg<0>(&issue));
  manager_.AddIssue(issue_info);
  EXPECT_EQ(issue_info, issue.info());

  // Adding the same IssueInfo has no effect.
  manager_.AddIssue(issue_info);

  EXPECT_CALL(observer, OnIssuesCleared());
  manager_.ClearIssue(issue.id());
  manager_.UnregisterObserver(&observer);
}

TEST_F(IssueManagerTest, NonBlockingIssuesGetAutoDismissed) {
  MockIssuesObserver observer(&router_);
  manager_.RegisterObserver(&observer);

  EXPECT_CALL(observer, OnIssue(_)).Times(1);
  IssueInfo issue_info1 = CreateTestIssue(IssueInfo::Severity::NOTIFICATION);
  manager_.AddIssue(issue_info1);

  EXPECT_CALL(observer, OnIssuesCleared()).Times(1);
  base::TimeDelta timeout = IssueManager::GetAutoDismissTimeout(issue_info1);
  EXPECT_FALSE(timeout.is_zero());
  EXPECT_TRUE(task_runner_->HasPendingTask());
  task_runner_->FastForwardBy(timeout);

  EXPECT_CALL(observer, OnIssue(_)).Times(1);
  IssueInfo issue_info2 = CreateTestIssue(IssueInfo::Severity::WARNING);
  manager_.AddIssue(issue_info2);

  EXPECT_CALL(observer, OnIssuesCleared()).Times(1);
  timeout = IssueManager::GetAutoDismissTimeout(issue_info2);
  EXPECT_FALSE(timeout.is_zero());
  EXPECT_TRUE(task_runner_->HasPendingTask());
  task_runner_->FastForwardBy(timeout);
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

TEST_F(IssueManagerTest, IssueAutoDismissNoopsIfAlreadyCleared) {
  MockIssuesObserver observer(&router_);
  manager_.RegisterObserver(&observer);

  Issue issue1((IssueInfo()));
  EXPECT_CALL(observer, OnIssue(_)).Times(1).WillOnce(SaveArg<0>(&issue1));
  IssueInfo issue_info1 = CreateTestIssue(IssueInfo::Severity::NOTIFICATION);
  manager_.AddIssue(issue_info1);

  EXPECT_CALL(observer, OnIssuesCleared()).Times(1);
  manager_.ClearIssue(issue1.id());

  EXPECT_CALL(observer, OnIssuesCleared()).Times(0);
  base::TimeDelta timeout = IssueManager::GetAutoDismissTimeout(issue_info1);
  EXPECT_FALSE(timeout.is_zero());
  EXPECT_TRUE(task_runner_->HasPendingTask());
  task_runner_->FastForwardBy(timeout);
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

TEST_F(IssueManagerTest, BlockingIssuesDoNotGetAutoDismissed) {
  MockIssuesObserver observer(&router_);
  manager_.RegisterObserver(&observer);

  EXPECT_CALL(observer, OnIssue(_)).Times(1);
  IssueInfo issue_info1 = CreateTestIssue(IssueInfo::Severity::WARNING);
  issue_info1.is_blocking = true;
  manager_.AddIssue(issue_info1);

  EXPECT_CALL(observer, OnIssuesCleared()).Times(0);

  base::TimeDelta timeout = IssueManager::GetAutoDismissTimeout(issue_info1);
  EXPECT_TRUE(timeout.is_zero());
  EXPECT_FALSE(task_runner_->HasPendingTask());

  // FATAL issues are always blocking.
  IssueInfo issue_info2 = CreateTestIssue(IssueInfo::Severity::FATAL);
  manager_.AddIssue(issue_info2);

  timeout = IssueManager::GetAutoDismissTimeout(issue_info2);
  EXPECT_TRUE(timeout.is_zero());
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

}  // namespace media_router
