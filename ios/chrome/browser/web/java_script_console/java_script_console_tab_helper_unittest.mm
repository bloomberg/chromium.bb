// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/java_script_console/java_script_console_tab_helper.h"

#include "base/values.h"
#import "ios/chrome/browser/web/chrome_web_client.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#include "ios/chrome/browser/web/java_script_console/java_script_console_message.h"
#include "ios/chrome/browser/web/java_script_console/java_script_console_tab_helper_delegate.h"
#include "ios/chrome/test/fakes/fake_java_script_console_tab_helper_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture to test JavaScriptConsoleTabHelper.
class JavaScriptConsoleTabHelperTest : public ChromeWebTest {
 protected:
  JavaScriptConsoleTabHelperTest()
      : ChromeWebTest(std::make_unique<ChromeWebClient>()) {}

  void SetUp() override {
    ChromeWebTest::SetUp();
    JavaScriptConsoleTabHelper::CreateForWebState(web_state());
  }

  // Loads the given HTML and initializes the JS scripts.
  void LoadHtml(NSString* html, GURL url) {
    ChromeWebTest::LoadHtml(html, url);
    ExecuteJavaScript(
        GetWebClient()->GetDocumentStartScriptForAllFrames(GetBrowserState()));
  }

  // Returns the JavaScriptConsoleTabHelper associated with |web_state()|.
  JavaScriptConsoleTabHelper* tab_helper() {
    return JavaScriptConsoleTabHelper::FromWebState(web_state());
  }
};

// Tests that a message can be logged without a
// JavaScriptConsoleTabHelperDelegate set.
TEST_F(JavaScriptConsoleTabHelperTest, LogMessageWithoutDelegate) {
  LoadHtml(@"<p></p>", GURL("http://chromium.test"));

  ASSERT_TRUE(tab_helper());

  // No need to verify state, but logging a message should not crash the
  // JavaScriptConsoleTabHelper when it has no delegate.
  ExecuteJavaScript(@"console.log('Log message');");
}

// Tests that a JavaScript console message is logged correctly.
TEST_F(JavaScriptConsoleTabHelperTest, LogMessage) {
  GURL url = GURL("http://chromium.test");
  LoadHtml(@"<p></p>", url);

  auto delegate = std::make_unique<FakeJavaScriptConsoleTabHelperDelegate>();
  ASSERT_TRUE(tab_helper());
  tab_helper()->SetDelegate(delegate.get());

  ASSERT_FALSE(delegate->GetLastLoggedMessage());

  ExecuteJavaScript(@"console.log('Log message');");

  const JavaScriptConsoleMessage* last_logged_message =
      delegate->GetLastLoggedMessage();
  ASSERT_TRUE(last_logged_message);
  EXPECT_EQ("log", last_logged_message->level);
  EXPECT_EQ(url, last_logged_message->url);
  EXPECT_EQ("Log message", last_logged_message->message->GetString());
}
