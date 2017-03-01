// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/web_state/web_state.h"

#include "base/mac/bind_objc_block.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#import "ios/web/public/test/web_test_with_web_state.h"

namespace web {

// Test fixture for web::WebTest class.
typedef web::WebTestWithWebState WebStateTest;

// Tests script execution with and without callback.
TEST_F(WebStateTest, ScriptExecution) {
  LoadHtml("<html></html>");

  // Execute script without callback.
  web_state()->ExecuteJavaScript(base::UTF8ToUTF16("window.foo = 'bar'"));

  // Execute script with callback.
  __block std::unique_ptr<base::Value> execution_result;
  __block bool execution_complete = false;
  web_state()->ExecuteJavaScript(base::UTF8ToUTF16("window.foo"),
                                 base::BindBlock(^(const base::Value* value) {
                                   execution_result = value->CreateDeepCopy();
                                   execution_complete = true;
                                 }));
  WaitForCondition(^{
    return execution_complete;
  });

  ASSERT_TRUE(execution_result);
  std::string string_result;
  execution_result->GetAsString(&string_result);
  EXPECT_EQ("bar", string_result);
}

// Tests loading progress.
TEST_F(WebStateTest, LoadingProgress) {
  EXPECT_FLOAT_EQ(0.0, web_state()->GetLoadingProgress());
  LoadHtml("<html></html>");
  WaitForCondition(^bool() {
    return web_state()->GetLoadingProgress() == 1.0;
  });
}

// Tests that page which overrides window.webkit object does not break the
// messaging system.
TEST_F(WebStateTest, OverridingWebKitObject) {
  // Add a script command handler.
  __block bool message_received = false;
  const web::WebState::ScriptCommandCallback callback =
      base::BindBlock(^bool(const base::DictionaryValue&, const GURL&, bool) {
        message_received = true;
        return true;
      });
  web_state()->AddScriptCommandCallback(callback, "test");

  // Load the page which overrides window.webkit object and wait until the
  // test message is received.
  LoadHtml(
      "<script>"
      "  webkit = undefined;"
      "  __gCrWeb.message.invokeOnHost({'command': 'test.webkit-overriding'});"
      "</script>");

  WaitForCondition(^{
    return message_received;
  });
  web_state()->RemoveScriptCommandCallback("test");
}

}  // namespace web
