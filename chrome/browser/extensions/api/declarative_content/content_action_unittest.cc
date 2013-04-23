// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/content_action.h"

#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/test_extension_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

using base::test::ParseJson;
using testing::HasSubstr;

TEST(DeclarativeContentActionTest, InvalidCreation) {
  TestExtensionEnvironment env;
  std::string error;
  bool bad_message = false;
  scoped_ptr<ContentAction> result;

  // Test wrong data type passed.
  error.clear();
  result = ContentAction::Create(*ParseJson("[]"), &error, &bad_message);
  EXPECT_TRUE(bad_message);
  EXPECT_EQ("", error);
  EXPECT_FALSE(result);

  // Test missing instanceType element.
  error.clear();
  result = ContentAction::Create(*ParseJson("{}"), &error, &bad_message);
  EXPECT_TRUE(bad_message);
  EXPECT_EQ("", error);
  EXPECT_FALSE(result);

  // Test wrong instanceType element.
  error.clear();
  result = ContentAction::Create(*ParseJson(
      "{\n"
      "  \"instanceType\": \"declarativeContent.UnknownType\",\n"
      "}"),
                                 &error, &bad_message);
  EXPECT_THAT(error, HasSubstr("invalid instanceType"));
  EXPECT_FALSE(result);
}

TEST(DeclarativeContentActionTest, ShowPageAction) {
  TestExtensionEnvironment env;

  std::string error;
  bool bad_message = false;
  scoped_ptr<ContentAction> result = ContentAction::Create(
      *ParseJson("{\n"
                 "  \"instanceType\": \"declarativeContent.ShowPageAction\",\n"
                 "}"),
      &error, &bad_message);
  EXPECT_EQ("", error);
  EXPECT_FALSE(bad_message);
  ASSERT_TRUE(result);
  EXPECT_EQ(ContentAction::ACTION_SHOW_PAGE_ACTION, result->GetType());

  const Extension* extension = env.MakeExtension(
      *ParseJson("{\"page_action\": { \"default_title\": \"Extension\" } }"));
  ExtensionAction* page_action =
      ExtensionActionManager::Get(env.profile())->GetPageAction(*extension);
  scoped_ptr<content::WebContents> contents = env.MakeTab();
  const int tab_id = ExtensionTabUtil::GetTabId(contents.get());
  EXPECT_FALSE(page_action->GetIsVisible(tab_id));
  ContentAction::ApplyInfo apply_info = {
    env.profile(), contents.get()
  };
  result->Apply(extension->id(), base::Time(), &apply_info);
  EXPECT_TRUE(page_action->GetIsVisible(tab_id));
  result->Apply(extension->id(), base::Time(), &apply_info);
  EXPECT_TRUE(page_action->GetIsVisible(tab_id));
  result->Revert(extension->id(), base::Time(), &apply_info);
  EXPECT_TRUE(page_action->GetIsVisible(tab_id));
  result->Revert(extension->id(), base::Time(), &apply_info);
  EXPECT_FALSE(page_action->GetIsVisible(tab_id));
}

}  // namespace
}  // namespace extensions
