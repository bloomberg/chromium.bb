// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative_content/content_action.h"

#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/common/extensions/value_builder.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

namespace extensions {
namespace {

using base::test::ParseJson;
using testing::HasSubstr;

class TestExtensionEnvironment {
 public:
  TestExtensionEnvironment()
      : ui_thread_(BrowserThread::UI, &loop_),
        file_thread_(BrowserThread::FILE),
        file_blocking_thread_(BrowserThread::FILE_USER_BLOCKING),
        io_thread_(BrowserThread::IO),
        profile_(new TestingProfile),
        extension_service_(NULL) {
    file_thread_.Start();
    file_blocking_thread_.Start();
    io_thread_.StartIOThread();
  }

  ~TestExtensionEnvironment() {
    profile_.reset();
    // Delete the profile, and then cycle the message loop to clear
    // out delayed deletions.
    base::RunLoop run_loop;
    loop_.PostTask(FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  TestingProfile* profile() { return profile_.get(); }

  ExtensionService* GetExtensionService() {
    if (extension_service_ == NULL) {
      TestExtensionSystem* extension_system =
          static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()));
      extension_service_ = extension_system->CreateExtensionService(
          CommandLine::ForCurrentProcess(), FilePath(), false);
    }
    return extension_service_;
  }

  scoped_refptr<Extension> MakeExtension(
      const DictionaryValue& manifest_extra) {
    scoped_ptr<base::DictionaryValue> manifest = DictionaryBuilder()
        .Set("name", "Extension")
        .Set("version", "1.0")
        .Set("manifest_version", 2)
        .Build();
    manifest->MergeDictionary(&manifest_extra);

    scoped_refptr<Extension> result =
        ExtensionBuilder().SetManifest(manifest.Pass()).Build();
    GetExtensionService()->AddExtension(result);
    return result;
  }

  scoped_ptr<content::WebContents> MakeTab() {
    scoped_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), NULL));
    // Create a tab id.
    SessionTabHelper::CreateForWebContents(contents.get());
    return contents.Pass();
  }

 private:
  MessageLoopForUI loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread file_blocking_thread_;
  content::TestBrowserThread io_thread_;
#if defined(OS_WIN)
  ui::ScopedOleInitializer ole_initializer_;
#endif
  scoped_ptr<TestingProfile> profile_;
  ExtensionService* extension_service_;
};

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

  scoped_refptr<Extension> extension = env.MakeExtension(
      *DictionaryBuilder().Set("page_action", DictionaryBuilder()
                               .Set("default_title", "Extension"))
      .Build());
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
