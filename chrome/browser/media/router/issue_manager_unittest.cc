// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/macros.h"
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
  IssueManagerTest() {}
  ~IssueManagerTest() override {}

  content::TestBrowserThreadBundle thread_bundle_;
  IssueManager manager_;
  MockMediaRouter router_;

 private:
  DISALLOW_COPY_AND_ASSIGN(IssueManagerTest);
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

}  // namespace media_router
