// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/user_script_loader.h"

#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using extensions::URLPatternSet;

namespace {

static void AddPattern(URLPatternSet* extent, const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  extent->AddPattern(URLPattern(schemes, pattern));
}
}

namespace extensions {

// Test bringing up a script loader on a specific directory, putting a script
// in there, etc.

class UserScriptLoaderTest : public testing::Test,
                             public content::NotificationObserver {
 public:
  UserScriptLoaderTest() : shared_memory_(NULL) {}

  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Register for all user script notifications.
    registrar_.Add(this,
                   extensions::NOTIFICATION_USER_SCRIPTS_UPDATED,
                   content::NotificationService::AllSources());

    // UserScriptLoader posts tasks to the file thread so make the current
    // thread look like one.
    file_thread_.reset(new content::TestBrowserThread(
        BrowserThread::FILE, base::MessageLoop::current()));
    ui_thread_.reset(new content::TestBrowserThread(
        BrowserThread::UI, base::MessageLoop::current()));
  }

  virtual void TearDown() {
    file_thread_.reset();
    ui_thread_.reset();
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    DCHECK(type == extensions::NOTIFICATION_USER_SCRIPTS_UPDATED);

    shared_memory_ = content::Details<base::SharedMemory>(details).ptr();
    if (base::MessageLoop::current() == &message_loop_)
      base::MessageLoop::current()->Quit();
  }

  // Directory containing user scripts.
  base::ScopedTempDir temp_dir_;

  content::NotificationRegistrar registrar_;

  // MessageLoop used in tests.
  base::MessageLoopForUI message_loop_;

  scoped_ptr<content::TestBrowserThread> file_thread_;
  scoped_ptr<content::TestBrowserThread> ui_thread_;

  // Updated to the script shared memory when we get notified.
  base::SharedMemory* shared_memory_;
};

// Test that we get notified even when there are no scripts.
TEST_F(UserScriptLoaderTest, NoScripts) {
  TestingProfile profile;
  UserScriptLoader loader(&profile,
                          std::string() /* owner_extension_id */,
                          true /* listen_for_extension_system_loaded */);
  loader.StartLoad();
  message_loop_.PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
  message_loop_.Run();

  ASSERT_TRUE(shared_memory_ != NULL);
}

TEST_F(UserScriptLoaderTest, Parse1) {
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
  EXPECT_TRUE(UserScriptLoader::ParseMetadataHeader(text, &script));
  ASSERT_EQ(3U, script.globs().size());
  EXPECT_EQ("*mail.google.com*", script.globs()[0]);
  EXPECT_EQ("*mail.yahoo.com*", script.globs()[1]);
  EXPECT_EQ("*mail.msn.com*", script.globs()[2]);
}

TEST_F(UserScriptLoaderTest, Parse2) {
  const std::string text("default to @include *");

  UserScript script;
  EXPECT_TRUE(UserScriptLoader::ParseMetadataHeader(text, &script));
  ASSERT_EQ(1U, script.globs().size());
  EXPECT_EQ("*", script.globs()[0]);
}

TEST_F(UserScriptLoaderTest, Parse3) {
  const std::string text(
    "// ==UserScript==\n"
    "// @include *foo*\n"
    "// ==/UserScript=="); // no trailing newline

  UserScript script;
  UserScriptLoader::ParseMetadataHeader(text, &script);
  ASSERT_EQ(1U, script.globs().size());
  EXPECT_EQ("*foo*", script.globs()[0]);
}

TEST_F(UserScriptLoaderTest, Parse4) {
  const std::string text(
    "// ==UserScript==\n"
    "// @match http://*.mail.google.com/*\n"
    "// @match  \t http://mail.yahoo.com/*\n"
    "// ==/UserScript==\n");

  URLPatternSet expected_patterns;
  AddPattern(&expected_patterns, "http://*.mail.google.com/*");
  AddPattern(&expected_patterns, "http://mail.yahoo.com/*");

  UserScript script;
  EXPECT_TRUE(UserScriptLoader::ParseMetadataHeader(text, &script));
  EXPECT_EQ(0U, script.globs().size());
  EXPECT_EQ(expected_patterns, script.url_patterns());
}

TEST_F(UserScriptLoaderTest, Parse5) {
  const std::string text(
    "// ==UserScript==\n"
    "// @match http://*mail.google.com/*\n"
    "// ==/UserScript==\n");

  // Invalid @match value.
  UserScript script;
  EXPECT_FALSE(UserScriptLoader::ParseMetadataHeader(text, &script));
}

TEST_F(UserScriptLoaderTest, Parse6) {
  const std::string text(
    "// ==UserScript==\n"
    "// @include http://*.mail.google.com/*\n"
    "// @match  \t http://mail.yahoo.com/*\n"
    "// ==/UserScript==\n");

  // Allowed to match @include and @match.
  UserScript script;
  EXPECT_TRUE(UserScriptLoader::ParseMetadataHeader(text, &script));
}

TEST_F(UserScriptLoaderTest, Parse7) {
  // Greasemonkey allows there to be any leading text before the comment marker.
  const std::string text(
    "// ==UserScript==\n"
    "adsasdfasf// @name hello\n"
    "  // @description\twiggity woo\n"
    "\t// @match  \t http://mail.yahoo.com/*\n"
    "// ==/UserScript==\n");

  UserScript script;
  EXPECT_TRUE(UserScriptLoader::ParseMetadataHeader(text, &script));
  ASSERT_EQ("hello", script.name());
  ASSERT_EQ("wiggity woo", script.description());
  ASSERT_EQ(1U, script.url_patterns().patterns().size());
  EXPECT_EQ("http://mail.yahoo.com/*",
            script.url_patterns().begin()->GetAsString());
}

TEST_F(UserScriptLoaderTest, Parse8) {
  const std::string text(
    "// ==UserScript==\n"
    "// @name myscript\n"
    "// @match http://www.google.com/*\n"
    "// @exclude_match http://www.google.com/foo*\n"
    "// ==/UserScript==\n");

  UserScript script;
  EXPECT_TRUE(UserScriptLoader::ParseMetadataHeader(text, &script));
  ASSERT_EQ("myscript", script.name());
  ASSERT_EQ(1U, script.url_patterns().patterns().size());
  EXPECT_EQ("http://www.google.com/*",
            script.url_patterns().begin()->GetAsString());
  ASSERT_EQ(1U, script.exclude_url_patterns().patterns().size());
  EXPECT_EQ("http://www.google.com/foo*",
            script.exclude_url_patterns().begin()->GetAsString());
}

TEST_F(UserScriptLoaderTest, SkipBOMAtTheBeginning) {
  base::FilePath path = temp_dir_.path().AppendASCII("script.user.js");
  const std::string content("\xEF\xBB\xBF alert('hello');");
  size_t written = base::WriteFile(path, content.c_str(), content.size());
  ASSERT_EQ(written, content.size());

  UserScript user_script;
  user_script.js_scripts().push_back(
      UserScript::File(temp_dir_.path(), path.BaseName(), GURL()));

  UserScriptList user_scripts;
  user_scripts.push_back(user_script);

  UserScriptLoader::LoadScriptsForTest(&user_scripts);

  EXPECT_EQ(content.substr(3),
            user_scripts[0].js_scripts()[0].GetContent().as_string());
}

TEST_F(UserScriptLoaderTest, LeaveBOMNotAtTheBeginning) {
  base::FilePath path = temp_dir_.path().AppendASCII("script.user.js");
  const std::string content("alert('here's a BOOM: \xEF\xBB\xBF');");
  size_t written = base::WriteFile(path, content.c_str(), content.size());
  ASSERT_EQ(written, content.size());

  UserScript user_script;
  user_script.js_scripts().push_back(UserScript::File(
      temp_dir_.path(), path.BaseName(), GURL()));

  UserScriptList user_scripts;
  user_scripts.push_back(user_script);

  UserScriptLoader::LoadScriptsForTest(&user_scripts);

  EXPECT_EQ(content, user_scripts[0].js_scripts()[0].GetContent().as_string());
}

}  // namespace extensions
