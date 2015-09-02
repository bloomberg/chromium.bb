// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/issue.h"
#include "chrome/grit/generated_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"

namespace media_router {

// Checks static IssueAction factory method.
TEST(IssueUnitTest, IssueActionConstructor) {
  // Pre-defined "Dismiss" issue action.
  IssueAction action1(IssueAction::TYPE_DISMISS);
  EXPECT_EQ(IssueAction::TYPE_DISMISS, action1.type());

  // Pre-defined "Learn More" issue action.
  IssueAction action2(IssueAction::TYPE_LEARN_MORE);
  EXPECT_EQ(IssueAction::TYPE_LEARN_MORE, action2.type());
}

// Tests custom Issue factory method without any secondary actions.
TEST(IssueUnitTest, CustomIssueConstructionWithNoSecondaryActions) {
  std::vector<IssueAction> secondary_actions;
  std::string title = "title";
  std::string message = "message";

  Issue issue1(title, message, IssueAction(IssueAction::TYPE_DISMISS),
               secondary_actions, "", Issue::WARNING, false,
               "http://www.google.com/");

  EXPECT_EQ(title, issue1.title());
  EXPECT_EQ(message, issue1.message());
  EXPECT_EQ(IssueAction::TYPE_DISMISS, issue1.default_action().type());
  EXPECT_TRUE(issue1.secondary_actions().empty());
  EXPECT_EQ(Issue::WARNING, issue1.severity());
  EXPECT_EQ("", issue1.route_id());
  EXPECT_TRUE(issue1.is_global());
  EXPECT_FALSE(issue1.is_blocking());
  EXPECT_EQ("http://www.google.com/", issue1.help_url().spec());

  Issue issue2(title, message, IssueAction(IssueAction::TYPE_DISMISS),
               secondary_actions, "routeid", Issue::FATAL, true,
               "http://www.google.com/");

  EXPECT_EQ(title, issue2.title());
  EXPECT_EQ(message, issue2.message());
  EXPECT_EQ(IssueAction::TYPE_DISMISS, issue1.default_action().type());
  EXPECT_TRUE(issue2.secondary_actions().empty());
  EXPECT_EQ(Issue::FATAL, issue2.severity());
  EXPECT_EQ("routeid", issue2.route_id());
  EXPECT_FALSE(issue2.is_global());
  EXPECT_TRUE(issue2.is_blocking());
  EXPECT_EQ("http://www.google.com/", issue1.help_url().spec());

  Issue issue3(title, "", IssueAction(IssueAction::TYPE_DISMISS),
               secondary_actions, "routeid", Issue::FATAL, true,
               "http://www.google.com/");

  EXPECT_EQ(title, issue3.title());
  EXPECT_EQ("", issue3.message());
  EXPECT_EQ(IssueAction::TYPE_DISMISS, issue1.default_action().type());
  EXPECT_TRUE(issue3.secondary_actions().empty());
  EXPECT_EQ(Issue::FATAL, issue3.severity());
  EXPECT_EQ("routeid", issue3.route_id());
  EXPECT_FALSE(issue3.is_global());
  EXPECT_TRUE(issue3.is_blocking());
  EXPECT_EQ("http://www.google.com/", issue1.help_url().spec());
}

// Tests custom Issue factory method with secondary actions.
TEST(IssueUnitTest, CustomIssueConstructionWithSecondaryActions) {
  std::vector<IssueAction> secondary_actions;
  secondary_actions.push_back(IssueAction(IssueAction::TYPE_DISMISS));
  EXPECT_EQ(1u, secondary_actions.size());
  std::string title = "title";
  std::string message = "message";

  Issue issue1(title, message, IssueAction(IssueAction::TYPE_LEARN_MORE),
               secondary_actions, "", Issue::WARNING, false, "");

  EXPECT_EQ(title, issue1.title());
  EXPECT_EQ(message, issue1.message());
  EXPECT_EQ(IssueAction::TYPE_LEARN_MORE, issue1.default_action().type());
  EXPECT_FALSE(issue1.secondary_actions().empty());
  EXPECT_EQ(1u, issue1.secondary_actions().size());
  EXPECT_EQ(Issue::WARNING, issue1.severity());
  EXPECT_EQ("", issue1.route_id());
  EXPECT_TRUE(issue1.is_global());
  EXPECT_FALSE(issue1.is_blocking());

  Issue issue2(title, message, IssueAction(IssueAction::TYPE_LEARN_MORE),
               secondary_actions, "routeid", Issue::FATAL, true, "");

  EXPECT_EQ(title, issue2.title());
  EXPECT_EQ(message, issue2.message());
  EXPECT_EQ(IssueAction::TYPE_LEARN_MORE, issue2.default_action().type());
  EXPECT_FALSE(issue2.secondary_actions().empty());
  EXPECT_EQ(1u, issue2.secondary_actions().size());
  EXPECT_EQ(Issue::FATAL, issue2.severity());
  EXPECT_EQ("routeid", issue2.route_id());
  EXPECT_FALSE(issue2.is_global());
  EXPECT_TRUE(issue2.is_blocking());

  Issue issue3(title, "", IssueAction(IssueAction::TYPE_LEARN_MORE),
               secondary_actions, "routeid", Issue::FATAL, true, "");

  EXPECT_EQ(title, issue3.title());
  EXPECT_EQ("", issue3.message());
  EXPECT_EQ(IssueAction::TYPE_LEARN_MORE, issue3.default_action().type());
  EXPECT_FALSE(issue3.secondary_actions().empty());
  EXPECT_EQ(1u, issue3.secondary_actions().size());
  EXPECT_EQ(Issue::FATAL, issue3.severity());
  EXPECT_EQ("routeid", issue3.route_id());
  EXPECT_FALSE(issue3.is_global());
  EXPECT_TRUE(issue3.is_blocking());
}

// Tests == and != method.
TEST(IssueUnitTest, Equal) {
  std::vector<IssueAction> secondary_actions;
  secondary_actions.push_back(IssueAction(IssueAction::TYPE_DISMISS));

  std::vector<IssueAction> secondary_actions2;

  std::string title = "title";
  std::string message = "message";

  Issue issue1(Issue(title, message, IssueAction(IssueAction::TYPE_LEARN_MORE),
                     secondary_actions, "", Issue::WARNING, false, ""));
  EXPECT_TRUE(issue1.Equals(issue1));

  Issue issue2(Issue(title, message, IssueAction(IssueAction::TYPE_LEARN_MORE),
                     secondary_actions, "", Issue::WARNING, false, ""));
  EXPECT_FALSE(issue1.Equals(issue2));
}

}  // namespace media_router
