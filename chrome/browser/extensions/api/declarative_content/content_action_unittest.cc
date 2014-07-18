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
#include "extensions/common/extension_builder.h"
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
  scoped_refptr<const ContentAction> result;

  // Test wrong data type passed.
  error.clear();
  result = ContentAction::Create(NULL, *ParseJson("[]"), &error, &bad_message);
  EXPECT_TRUE(bad_message);
  EXPECT_EQ("", error);
  EXPECT_FALSE(result.get());

  // Test missing instanceType element.
  error.clear();
  result = ContentAction::Create(NULL, *ParseJson("{}"), &error, &bad_message);
  EXPECT_TRUE(bad_message);
  EXPECT_EQ("", error);
  EXPECT_FALSE(result.get());

  // Test wrong instanceType element.
  error.clear();
  result = ContentAction::Create(NULL, *ParseJson(
      "{\n"
      "  \"instanceType\": \"declarativeContent.UnknownType\",\n"
      "}"),
                                 &error, &bad_message);
  EXPECT_THAT(error, HasSubstr("invalid instanceType"));
  EXPECT_FALSE(result.get());
}

TEST(DeclarativeContentActionTest, ShowPageActionWithoutPageAction) {
  TestExtensionEnvironment env;

  const Extension* extension = env.MakeExtension(base::DictionaryValue());
  std::string error;
  bool bad_message = false;
  scoped_refptr<const ContentAction> result = ContentAction::Create(
      extension,
      *ParseJson(
           "{\n"
           "  \"instanceType\": \"declarativeContent.ShowPageAction\",\n"
           "}"),
      &error,
      &bad_message);
  EXPECT_THAT(error, testing::HasSubstr("without a page action"));
  EXPECT_FALSE(bad_message);
  ASSERT_FALSE(result.get());
}

TEST(DeclarativeContentActionTest, ShowPageAction) {
  TestExtensionEnvironment env;

  const Extension* extension = env.MakeExtension(
      *ParseJson("{\"page_action\": { \"default_title\": \"Extension\" } }"));
  std::string error;
  bool bad_message = false;
  scoped_refptr<const ContentAction> result = ContentAction::Create(
      extension,
      *ParseJson(
           "{\n"
           "  \"instanceType\": \"declarativeContent.ShowPageAction\",\n"
           "}"),
      &error,
      &bad_message);
  EXPECT_EQ("", error);
  EXPECT_FALSE(bad_message);
  ASSERT_TRUE(result.get());
  EXPECT_EQ(ContentAction::ACTION_SHOW_PAGE_ACTION, result->GetType());

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

TEST(DeclarativeContentActionTest, RequestContentScriptMissingScripts) {
  TestExtensionEnvironment env;

  std::string error;
  bool bad_message = false;
  scoped_refptr<const ContentAction> result = ContentAction::Create(
      NULL,
      *ParseJson(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"allFrames\": true,\n"
          "  \"matchAboutBlank\": true\n"
          "}"),
      &error,
      &bad_message);
  EXPECT_THAT(error, testing::HasSubstr("Missing parameter is required"));
  EXPECT_FALSE(bad_message);
  ASSERT_FALSE(result.get());
}

TEST(DeclarativeContentActionTest, RequestContentScriptCSS) {
  TestExtensionEnvironment env;

  std::string error;
  bool bad_message = false;
  scoped_refptr<const ContentAction> result = ContentAction::Create(
      NULL,
      *ParseJson(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"css\": [\"style.css\"]\n"
          "}"),
      &error,
      &bad_message);
  EXPECT_EQ("", error);
  EXPECT_FALSE(bad_message);
  ASSERT_TRUE(result.get());
  EXPECT_EQ(ContentAction::ACTION_REQUEST_CONTENT_SCRIPT, result->GetType());
}

TEST(DeclarativeContentActionTest, RequestContentScriptJS) {
  TestExtensionEnvironment env;

  std::string error;
  bool bad_message = false;
  scoped_refptr<const ContentAction> result = ContentAction::Create(
      NULL,
      *ParseJson(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"js\": [\"script.js\"]\n"
          "}"),
      &error,
      &bad_message);
  EXPECT_EQ("", error);
  EXPECT_FALSE(bad_message);
  ASSERT_TRUE(result.get());
  EXPECT_EQ(ContentAction::ACTION_REQUEST_CONTENT_SCRIPT, result->GetType());
}

TEST(DeclarativeContentActionTest, RequestContentScriptCSSBadType) {
  TestExtensionEnvironment env;

  std::string error;
  bool bad_message = false;
  scoped_refptr<const ContentAction> result = ContentAction::Create(
      NULL,
      *ParseJson(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"css\": \"style.css\"\n"
          "}"),
      &error,
      &bad_message);
  EXPECT_TRUE(bad_message);
  ASSERT_FALSE(result.get());
}

TEST(DeclarativeContentActionTest, RequestContentScriptJSBadType) {
  TestExtensionEnvironment env;

  std::string error;
  bool bad_message = false;
  scoped_refptr<const ContentAction> result = ContentAction::Create(
      NULL,
      *ParseJson(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"js\": \"script.js\"\n"
          "}"),
      &error,
      &bad_message);
  EXPECT_TRUE(bad_message);
  ASSERT_FALSE(result.get());
}

TEST(DeclarativeContentActionTest, RequestContentScriptAllFrames) {
  TestExtensionEnvironment env;

  std::string error;
  bool bad_message = false;
  scoped_refptr<const ContentAction> result = ContentAction::Create(
      NULL,
      *ParseJson(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"js\": [\"script.js\"],\n"
          "  \"allFrames\": true\n"
          "}"),
      &error,
      &bad_message);
  EXPECT_EQ("", error);
  EXPECT_FALSE(bad_message);
  ASSERT_TRUE(result.get());
  EXPECT_EQ(ContentAction::ACTION_REQUEST_CONTENT_SCRIPT, result->GetType());
}

TEST(DeclarativeContentActionTest, RequestContentScriptMatchAboutBlank) {
  TestExtensionEnvironment env;

  std::string error;
  bool bad_message = false;
  scoped_refptr<const ContentAction> result = ContentAction::Create(
      NULL,
      *ParseJson(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"js\": [\"script.js\"],\n"
          "  \"matchAboutBlank\": true\n"
          "}"),
      &error,
      &bad_message);
  EXPECT_EQ("", error);
  EXPECT_FALSE(bad_message);
  ASSERT_TRUE(result.get());
  EXPECT_EQ(ContentAction::ACTION_REQUEST_CONTENT_SCRIPT, result->GetType());
}

TEST(DeclarativeContentActionTest, RequestContentScriptAllFramesBadType) {
  TestExtensionEnvironment env;

  std::string error;
  bool bad_message = false;
  scoped_refptr<const ContentAction> result = ContentAction::Create(
      NULL,
      *ParseJson(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"js\": [\"script.js\"],\n"
          "  \"allFrames\": null\n"
          "}"),
      &error,
      &bad_message);
  EXPECT_TRUE(bad_message);
  ASSERT_FALSE(result.get());
}

TEST(DeclarativeContentActionTest, RequestContentScriptMatchAboutBlankBadType) {
  TestExtensionEnvironment env;

  std::string error;
  bool bad_message = false;
  scoped_refptr<const ContentAction> result = ContentAction::Create(
      NULL,
      *ParseJson(
          "{\n"
          "  \"instanceType\": \"declarativeContent.RequestContentScript\",\n"
          "  \"js\": [\"script.js\"],\n"
          "  \"matchAboutBlank\": null\n"
          "}"),
      &error,
      &bad_message);
  EXPECT_TRUE(bad_message);
  ASSERT_FALSE(result.get());
}

}  // namespace
}  // namespace extensions
