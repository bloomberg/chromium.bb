// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "chrome/browser/media/router/issue_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_router {
namespace {

const char kTestRouteId[] = "routeId";

Issue CreateTestIssue(const std::string& route_id) {
  return Issue("title", "message", IssueAction(IssueAction::TYPE_DISMISS),
               std::vector<IssueAction>(), route_id, Issue::WARNING, false,
               "http://www.example.com/help");
}

class IssueManagerUnitTest : public ::testing::Test {
 protected:
  IssueManagerUnitTest() {}
  ~IssueManagerUnitTest() override {}

  IssueManager manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(IssueManagerUnitTest);
};

TEST_F(IssueManagerUnitTest, InitializeManager) {
  // Before anything is done to the manager, it should hold no issues.
  EXPECT_EQ(0u, manager_.GetIssueCount());
}

TEST_F(IssueManagerUnitTest, AddIssue) {
  Issue issue = CreateTestIssue(kTestRouteId);

  // Add initial issue.
  manager_.AddIssue(issue);
  EXPECT_EQ(1u, manager_.GetIssueCount());

  // Attempt to add the same issue. Duplicates should not be inserted.
  manager_.AddIssue(issue);
  EXPECT_EQ(1u, manager_.GetIssueCount());
}

TEST_F(IssueManagerUnitTest, ClearIssue) {
  Issue issue = CreateTestIssue(kTestRouteId);

  // Remove an issue that doesn't exist.
  manager_.ClearIssue("id");

  // Add initial issue.
  manager_.AddIssue(issue);
  EXPECT_EQ(1u, manager_.GetIssueCount());

  // Remove the only issue.
  manager_.ClearIssue(issue.id());
  EXPECT_EQ(0u, manager_.GetIssueCount());

  // Remove an issue that doesn't exist.
  manager_.ClearIssue("id");
  EXPECT_EQ(0u, manager_.GetIssueCount());
}

TEST_F(IssueManagerUnitTest, ClearAllIssues) {
  // Add ten issues.
  for (int i = 0; i < 10; i++) {
    manager_.AddIssue(CreateTestIssue(kTestRouteId));
  }

  // Check that the issues were added.
  EXPECT_EQ(10u, manager_.GetIssueCount());

  // Remove all the issues.
  manager_.ClearAllIssues();
  EXPECT_EQ(0u, manager_.GetIssueCount());
}

TEST_F(IssueManagerUnitTest, ClearGlobalIssues) {
  // Add ten non-global issues.
  for (int i = 0; i < 10; i++) {
    manager_.AddIssue(CreateTestIssue(kTestRouteId));
  }

  // Check that the issues were added.
  EXPECT_EQ(10u, manager_.GetIssueCount());

  // Add five global issues.
  for (int i = 0; i < 5; i++) {
    manager_.AddIssue(CreateTestIssue(""));
  }

  // Check that the issues were added.
  EXPECT_EQ(15u, manager_.GetIssueCount());

  // Remove all the global issues.
  manager_.ClearGlobalIssues();
  EXPECT_EQ(10u, manager_.GetIssueCount());
}

TEST_F(IssueManagerUnitTest, ClearIssuesWithRouteId) {
  const std::string route_id_one = "route_id1";
  const std::string route_id_two = "route_id2";

  // Add ten issues with the same route.
  for (int i = 0; i < 10; i++) {
    manager_.AddIssue(CreateTestIssue(route_id_one));
  }

  // Check that the issues were added.
  EXPECT_EQ(10u, manager_.GetIssueCount());

  // Add ten issues with a different route.
  for (int i = 0; i < 10; i++) {
    manager_.AddIssue(CreateTestIssue(route_id_two));
  }

  // Check that the issues were added.
  EXPECT_EQ(20u, manager_.GetIssueCount());

  // Add ten global issues.
  for (int i = 0; i < 10; i++) {
    manager_.AddIssue(CreateTestIssue(""));
  }

  // Check that the issues were added.
  EXPECT_EQ(30u, manager_.GetIssueCount());

  // Remove all routes with route_id_one.
  manager_.ClearIssuesWithRouteId(route_id_one);
  EXPECT_EQ(20u, manager_.GetIssueCount());

  // Remove all routes with route_id_two.
  manager_.ClearIssuesWithRouteId(route_id_two);
  EXPECT_EQ(10u, manager_.GetIssueCount());
}

}  // namespace
}  // namespace media_router
