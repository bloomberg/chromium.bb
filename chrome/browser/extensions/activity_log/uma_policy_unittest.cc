// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "chrome/browser/extensions/activity_log/uma_policy.h"
#include "chrome/test/base/testing_profile.h"
#include "extensions/common/dom_action_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class UmaPolicyTest : public testing::Test {
 public:
  UmaPolicyTest() {
    profile_.reset(new TestingProfile());
  }

 protected:
  scoped_ptr<TestingProfile> profile_;
};

TEST_F(UmaPolicyTest, Construct) {
  ActivityLogPolicy* policy = new UmaPolicy(profile_.get());
  policy->Close();
}

TEST_F(UmaPolicyTest, MatchActionToStatusTest) {
  UmaPolicy* policy = new UmaPolicy(profile_.get());

  scoped_refptr<Action> action = new Action(
      "id", base::Time::Now(), Action::ACTION_API_CALL, "extension.connect");
  ASSERT_EQ(UmaPolicy::NONE, policy->MatchActionToStatus(action));

  action = new Action(
      "id", base::Time::Now(), Action::ACTION_API_CALL, "tabs.executeScript");
  ASSERT_EQ(
      (1 << UmaPolicy::CONTENT_SCRIPT), policy->MatchActionToStatus(action));

  action = new Action(
      "id", base::Time::Now(), Action::ACTION_CONTENT_SCRIPT, "");
  ASSERT_TRUE(
      (1 << UmaPolicy::CONTENT_SCRIPT) & policy->MatchActionToStatus(action));
  ASSERT_EQ(
      (1 << UmaPolicy::CONTENT_SCRIPT), policy->MatchActionToStatus(action));

  action = new Action(
      "id", base::Time::Now(), Action::ACTION_DOM_ACCESS, "Document.location");
  action->mutable_other()->SetInteger(activity_log_constants::kActionDomVerb,
                                      DomActionType::GETTER);
  ASSERT_TRUE((1 << UmaPolicy::READ_DOM) & policy->MatchActionToStatus(action));
  ASSERT_EQ((1 << UmaPolicy::READ_DOM), policy->MatchActionToStatus(action));

  action->mutable_other()->SetInteger(activity_log_constants::kActionDomVerb,
                                      DomActionType::SETTER);
  ASSERT_TRUE(
      (1 << UmaPolicy::MODIFIED_DOM) & policy->MatchActionToStatus(action));
  ASSERT_EQ(
      (1 << UmaPolicy::MODIFIED_DOM), policy->MatchActionToStatus(action));

  action = new Action(
      "id", base::Time::Now(), Action::ACTION_DOM_ACCESS, "HTMLDocument.write");
  action->mutable_other()->SetInteger(activity_log_constants::kActionDomVerb,
                                      DomActionType::METHOD);
  ASSERT_TRUE(
      (1 << UmaPolicy::DOCUMENT_WRITE) & policy->MatchActionToStatus(action));
  ASSERT_TRUE(
      (1 << UmaPolicy::DOM_METHOD) & policy->MatchActionToStatus(action));

  action = new Action("id",
                      base::Time::Now(),
                      Action::ACTION_DOM_ACCESS,
                      "Document.createElement");
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Set(0, new base::StringValue("script"));
  action->set_args(args.Pass());
  ASSERT_EQ(UmaPolicy::NONE, policy->MatchActionToStatus(action));
  action->mutable_other()->SetInteger(activity_log_constants::kActionDomVerb,
                                      DomActionType::METHOD);
  ASSERT_TRUE(
      (1 << UmaPolicy::CREATED_SCRIPT) & policy->MatchActionToStatus(action));
  ASSERT_TRUE(
      (1 << UmaPolicy::DOM_METHOD) & policy->MatchActionToStatus(action));

  policy->Close();
}

TEST_F(UmaPolicyTest, SiteUrlTest) {
  UmaPolicy* policy = new UmaPolicy(profile_.get());

  const std::string site0 = "http://www.zzz.com/";
  const std::string site1 = "http://www.foo.com/";
  const std::string site2 = "http://www.google.com#a";
  const std::string site3 = "http://www.google.com#bb";

  // Record some opened sites.
  policy->SetupOpenedPage(site1);
  policy->SetupOpenedPage(site2);
  policy->SetupOpenedPage(site2);
  policy->SetupOpenedPage(site3);
  policy->SetupOpenedPage(site3);
  policy->SetupOpenedPage(site3);

  // Check that site1, site2, and site3 were recorded.
  ASSERT_EQ(3u, policy->url_status().size());
  ASSERT_EQ(1, policy->url_status()[site1][UmaPolicy::kNumberOfTabs]);
  ASSERT_EQ(2, policy->url_status()[site2][UmaPolicy::kNumberOfTabs]);
  ASSERT_EQ(3, policy->url_status()[site3][UmaPolicy::kNumberOfTabs]);

  // Remove some sites.
  policy->CleanupClosedPage(site0, NULL);
  policy->CleanupClosedPage(site2, NULL);
  policy->CleanupClosedPage(site2, NULL);
  policy->CleanupClosedPage(site3, NULL);

  // Check that the removal worked.
  ASSERT_EQ(2u, policy->url_status().size());
  ASSERT_EQ(1, policy->url_status()[site1][UmaPolicy::kNumberOfTabs]);
  ASSERT_EQ(2, policy->url_status()[site3][UmaPolicy::kNumberOfTabs]);

  policy->Close();
}

TEST_F(UmaPolicyTest, ProcessActionTest) {
  const std::string site_a = "http://www.zzz.com/";
  const std::string site_b = "http://www.foo.com/";
  const std::string ext_a = "a";
  const std::string ext_b = "b";
  UmaPolicy* policy = new UmaPolicy(profile_.get());

  // Populate with a few different pages.
  policy->SetupOpenedPage(site_a);
  policy->SetupOpenedPage(site_b);

  // Process a few actions for site_a.
  scoped_refptr<Action> action1 = new Action(
      ext_a, base::Time::Now(), Action::ACTION_CONTENT_SCRIPT, "");
  action1->set_page_url(GURL(site_a));
  policy->ProcessAction(action1);

  scoped_refptr<Action> action2 = new Action(
      ext_a, base::Time::Now(), Action::ACTION_CONTENT_SCRIPT, "");
  action2->set_page_url(GURL(site_a));
  policy->ProcessAction(action2);

  scoped_refptr<Action> action3 = new Action(
      ext_b, base::Time::Now(), Action::ACTION_DOM_ACCESS, "Document.location");
  action3->mutable_other()->SetInteger(activity_log_constants::kActionDomVerb,
                                       DomActionType::GETTER);
  action3->set_page_url(GURL(site_a));
  policy->ProcessAction(action3);

  // Process an action for site_b.
  scoped_refptr<Action> action4 = new Action(
      ext_a, base::Time::Now(), Action::ACTION_DOM_ACCESS, "Document.location");
  action4->mutable_other()->SetInteger(activity_log_constants::kActionDomVerb,
                                       DomActionType::SETTER);
  action4->set_page_url(GURL(site_b));
  policy->ProcessAction(action4);

  scoped_refptr<Action> action5 = new Action(
      ext_b, base::Time::Now(), Action::ACTION_DOM_ACCESS, "Document.location");
  action5->mutable_other()->SetInteger(activity_log_constants::kActionDomVerb,
                                       DomActionType::SETTER);
  action5->set_page_url(GURL(site_b));
  policy->ProcessAction(action5);

  scoped_refptr<Action> action6 = new Action(
      ext_b, base::Time::Now(), Action::ACTION_API_CALL, "tabs.executeScript");
  action6->set_arg_url(GURL(site_b));
  policy->ProcessAction(action6);

  // Now check what's been recorded.
  ASSERT_EQ(2u, policy->url_status().size());

  ASSERT_EQ(3u, policy->url_status()[site_a].size());
  ASSERT_TRUE(
      (1 << UmaPolicy::CONTENT_SCRIPT) & policy->url_status()[site_a][ext_a]);
  ASSERT_FALSE(
      (1 << UmaPolicy::CONTENT_SCRIPT) & policy->url_status()[site_a][ext_b]);
  ASSERT_TRUE((1 << UmaPolicy::READ_DOM) & policy->url_status()[site_a][ext_b]);
  ASSERT_FALSE(
      (1 << UmaPolicy::READ_DOM) & policy->url_status()[site_a][ext_a]);

  ASSERT_EQ(3u, policy->url_status()[site_b].size());
  ASSERT_TRUE(
      (1 << UmaPolicy::MODIFIED_DOM) & policy->url_status()[site_b][ext_a]);
  ASSERT_TRUE(
      (1 << UmaPolicy::MODIFIED_DOM) & policy->url_status()[site_b][ext_b]);
  ASSERT_TRUE(
      (1 << UmaPolicy::CONTENT_SCRIPT) & policy->url_status()[site_b][ext_b]);

  policy->Close();
}

TEST_F(UmaPolicyTest, CleanURLTest) {
  ASSERT_EQ("http://www.google.com/",
            UmaPolicy::CleanURL(GURL("http://www.google.com/")));
  ASSERT_EQ("http://www.google.com/",
            UmaPolicy::CleanURL(GURL("http://www.google.com")));
  ASSERT_EQ("http://www.google.com:8080/a.html",
            UmaPolicy::CleanURL(GURL("http://www.google.com:8080/a.html")));
  ASSERT_EQ("http://www.google.com/",
            UmaPolicy::CleanURL(GURL("http://www.google.com/#a")));
  ASSERT_EQ("http://www.google.com/",
            UmaPolicy::CleanURL(GURL("http://www.google.com/#aaaa")));
  ASSERT_EQ("http://www.google.com/?q=a",
            UmaPolicy::CleanURL(GURL("http://www.google.com/?q=a")));

  ASSERT_EQ("http://www.cnn.com/",
            UmaPolicy::CleanURL(GURL("http://www.cnn.com/")));
  ASSERT_EQ("http://www.cnn.com:8080/a.html",
            UmaPolicy::CleanURL(GURL("http://www.cnn.com:8080/a.html")));
  ASSERT_EQ("http://www.cnn.com/",
            UmaPolicy::CleanURL(GURL("http://www.cnn.com")));
  ASSERT_EQ("http://www.cnn.com/",
            UmaPolicy::CleanURL(GURL("http://www.cnn.com/#a")));
  ASSERT_EQ("http://www.cnn.com/",
            UmaPolicy::CleanURL(GURL("http://www.cnn.com/#aaaa")));
  ASSERT_EQ("http://www.cnn.com/?q=a",
            UmaPolicy::CleanURL(GURL("http://www.cnn.com/?q=a")));
}

}  // namespace extensions
