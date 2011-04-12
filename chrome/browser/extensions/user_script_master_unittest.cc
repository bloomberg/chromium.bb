// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/user_script_master.h"

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/memory/scoped_temp_dir.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test bringing up a master on a specific directory, putting a script
// in there, etc.

class UserScriptMasterTest : public testing::Test,
                             public NotificationObserver {
 public:
  UserScriptMasterTest()
      : message_loop_(MessageLoop::TYPE_UI),
        shared_memory_(NULL) {
  }

  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Register for all user script notifications.
    registrar_.Add(this, NotificationType::USER_SCRIPTS_UPDATED,
                   NotificationService::AllSources());

    // UserScriptMaster posts tasks to the file thread so make the current
    // thread look like one.
    file_thread_.reset(new BrowserThread(
        BrowserThread::FILE, MessageLoop::current()));
  }

  virtual void TearDown() {
    file_thread_.reset();
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    DCHECK(type == NotificationType::USER_SCRIPTS_UPDATED);

    shared_memory_ = Details<base::SharedMemory>(details).ptr();
    if (MessageLoop::current() == &message_loop_)
      MessageLoop::current()->Quit();
  }

  // Directory containing user scripts.
  ScopedTempDir temp_dir_;

  NotificationRegistrar registrar_;

  // MessageLoop used in tests.
  MessageLoop message_loop_;

  scoped_ptr<BrowserThread> file_thread_;

  // Updated to the script shared memory when we get notified.
  base::SharedMemory* shared_memory_;
};

// Test that we get notified even when there are no scripts.
TEST_F(UserScriptMasterTest, NoScripts) {
  TestingProfile profile;
  scoped_refptr<UserScriptMaster> master(new UserScriptMaster(temp_dir_.path(),
                                                              &profile));
  master->StartScan();
  message_loop_.PostTask(FROM_HERE, new MessageLoop::QuitTask);
  message_loop_.Run();

  ASSERT_TRUE(shared_memory_ != NULL);
}

// Test that we get notified about scripts if they're already in the test dir.
TEST_F(UserScriptMasterTest, ExistingScripts) {
  TestingProfile profile;
  FilePath path = temp_dir_.path().AppendASCII("script.user.js");

  const char content[] = "some content";
  size_t written = file_util::WriteFile(path, content, sizeof(content));
  ASSERT_EQ(written, sizeof(content));

  scoped_refptr<UserScriptMaster> master(new UserScriptMaster(temp_dir_.path(),
                                                              &profile));
  master->StartScan();

  message_loop_.PostTask(FROM_HERE, new MessageLoop::QuitTask);
  message_loop_.Run();

  ASSERT_TRUE(shared_memory_ != NULL);
}

TEST_F(UserScriptMasterTest, Parse1) {
  const std::string text(
    "// This is my awesome script\n"
    "// It does stuff.\n"
    "// ==UserScript==   trailing garbage\n"
    "// @name foobar script\n"
    "// @namespace http://www.google.com/\n"
    "// @include *mail.google.com*\n"
    "// \n"
    "// @othergarbage\n"
    "// @include *mail.yahoo.com*\r\n"
    "// @include  \t *mail.msn.com*\n" // extra spaces after "@include" OK
    "//@include not-recognized\n" // must have one space after "//"
    "// ==/UserScript==  trailing garbage\n"
    "\n"
    "\n"
    "alert('hoo!');\n");

  UserScript script;
  EXPECT_TRUE(UserScriptMaster::ScriptReloader::ParseMetadataHeader(
      text, &script));
  ASSERT_EQ(3U, script.globs().size());
  EXPECT_EQ("*mail.google.com*", script.globs()[0]);
  EXPECT_EQ("*mail.yahoo.com*", script.globs()[1]);
  EXPECT_EQ("*mail.msn.com*", script.globs()[2]);
}

TEST_F(UserScriptMasterTest, Parse2) {
  const std::string text("default to @include *");

  UserScript script;
  EXPECT_TRUE(UserScriptMaster::ScriptReloader::ParseMetadataHeader(
      text, &script));
  ASSERT_EQ(1U, script.globs().size());
  EXPECT_EQ("*", script.globs()[0]);
}

TEST_F(UserScriptMasterTest, Parse3) {
  const std::string text(
    "// ==UserScript==\n"
    "// @include *foo*\n"
    "// ==/UserScript=="); // no trailing newline

  UserScript script;
  UserScriptMaster::ScriptReloader::ParseMetadataHeader(text, &script);
  ASSERT_EQ(1U, script.globs().size());
  EXPECT_EQ("*foo*", script.globs()[0]);
}

TEST_F(UserScriptMasterTest, Parse4) {
  const std::string text(
    "// ==UserScript==\n"
    "// @match http://*.mail.google.com/*\n"
    "// @match  \t http://mail.yahoo.com/*\n"
    "// ==/UserScript==\n");

  UserScript script;
  EXPECT_TRUE(UserScriptMaster::ScriptReloader::ParseMetadataHeader(
      text, &script));
  EXPECT_EQ(0U, script.globs().size());
  ASSERT_EQ(2U, script.url_patterns().size());
  EXPECT_EQ("http://*.mail.google.com/*",
            script.url_patterns()[0].GetAsString());
  EXPECT_EQ("http://mail.yahoo.com/*",
            script.url_patterns()[1].GetAsString());
}

TEST_F(UserScriptMasterTest, Parse5) {
  const std::string text(
    "// ==UserScript==\n"
    "// @match http://*mail.google.com/*\n"
    "// ==/UserScript==\n");

  // Invalid @match value.
  UserScript script;
  EXPECT_FALSE(UserScriptMaster::ScriptReloader::ParseMetadataHeader(
      text, &script));
}

TEST_F(UserScriptMasterTest, Parse6) {
  const std::string text(
    "// ==UserScript==\n"
    "// @include http://*.mail.google.com/*\n"
    "// @match  \t http://mail.yahoo.com/*\n"
    "// ==/UserScript==\n");

  // Allowed to match @include and @match.
  UserScript script;
  EXPECT_TRUE(UserScriptMaster::ScriptReloader::ParseMetadataHeader(
      text, &script));
}

TEST_F(UserScriptMasterTest, Parse7) {
  // Greasemonkey allows there to be any leading text before the comment marker.
  const std::string text(
    "// ==UserScript==\n"
    "adsasdfasf// @name hello\n"
    "  // @description\twiggity woo\n"
    "\t// @match  \t http://mail.yahoo.com/*\n"
    "// ==/UserScript==\n");

  UserScript script;
  EXPECT_TRUE(UserScriptMaster::ScriptReloader::ParseMetadataHeader(
      text, &script));
  ASSERT_EQ("hello", script.name());
  ASSERT_EQ("wiggity woo", script.description());
  ASSERT_EQ(1U, script.url_patterns().size());
  EXPECT_EQ("http://mail.yahoo.com/*",
            script.url_patterns()[0].GetAsString());
}

TEST_F(UserScriptMasterTest, SkipBOMAtTheBeginning) {
  FilePath path = temp_dir_.path().AppendASCII("script.user.js");

  const std::string content(
    "\xEF\xBB\xBF// ==UserScript==\n"
    "// @match http://*.mail.google.com/*\n"
    "// ==/UserScript==\n");
  size_t written = file_util::WriteFile(path, content.c_str(), content.size());
  ASSERT_EQ(written, content.size());

  UserScriptList script_list;
  UserScriptMaster::ScriptReloader::LoadScriptsFromDirectory(
      temp_dir_.path(), &script_list);
  ASSERT_EQ(1U, script_list.size());

  EXPECT_EQ(content.substr(3),
            script_list[0].js_scripts()[0].GetContent().as_string());
  EXPECT_EQ("http://*.mail.google.com/*",
            script_list[0].url_patterns()[0].GetAsString());
}

TEST_F(UserScriptMasterTest, LeaveBOMNotAtTheBeginning) {
  FilePath path = temp_dir_.path().AppendASCII("script.user.js");

  const std::string content(
    "// ==UserScript==\n"
    "// @match http://*.mail.google.com/*\n"
    "// ==/UserScript==\n"
    "// @bom \xEF\xBB\xBF");
  size_t written = file_util::WriteFile(path, content.c_str(), content.size());
  ASSERT_EQ(written, content.size());

  UserScriptList script_list;
  UserScriptMaster::ScriptReloader::LoadScriptsFromDirectory(
      temp_dir_.path(), &script_list);
  ASSERT_EQ(1U, script_list.size());

  EXPECT_EQ(content, script_list[0].js_scripts()[0].GetContent().as_string());
  EXPECT_EQ("http://*.mail.google.com/*",
            script_list[0].url_patterns()[0].GetAsString());
}
