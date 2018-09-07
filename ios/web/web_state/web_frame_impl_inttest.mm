// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/web_state/web_frame_impl.h"

#include "base/ios/ios_util.h"
#import "base/test/ios/wait_util.mm"
#import "ios/web/public/test/fakes/test_web_client.h"
#import "ios/web/public/test/web_js_test.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "ios/web/web_state/web_frames_manager_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {
// Waits for the WebFrame respresenting the main frame in the given
// |web_state|. Returns nullptr if a timeout of kWaitForJSCompletionTimeout
// occurs before the WebFrame exists.
web::WebFrame* GetMainWebFrameForWebState(web::WebState* web_state) {
  __block web::WebFramesManagerImpl* manager =
      web::WebFramesManagerImpl::FromWebState(web_state);
  __unused bool success =
      WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
        return manager->GetMainWebFrame();
      });
  return manager->GetMainWebFrame();
}

// Returns the first WebFrame found which is not the main frame in the given
// |web_state|. Does not wait and returns null if such a frame is not found.
web::WebFrame* GetChildWebFrameForWebState(web::WebState* web_state) {
  __block web::WebFramesManagerImpl* manager =
      web::WebFramesManagerImpl::FromWebState(web_state);
  web::WebFrame* iframe = nullptr;
  for (web::WebFrame* frame : manager->GetAllWebFrames()) {
    if (!frame->IsMainFrame()) {
      iframe = frame;
      break;
    }
  }
  return iframe;
}
}

namespace web {

// Test fixture to test WebFrameImpl with a real JavaScript context.
typedef web::WebTestWithWebState WebFrameImplIntTest;

// Tests that the expected result is received from executing a JavaScript
// function via |CallJavaScriptFunction| on the main frame.
TEST_F(WebFrameImplIntTest, CallJavaScriptFunctionOnMainFrame) {
  if (!base::ios::IsRunningOnIOS11OrLater()) {
    // Frame messaging is not supported on iOS 10.
    return;
  }

  ASSERT_TRUE(LoadHtml("<p>"));

  WebFrame* main_frame = GetMainWebFrameForWebState(web_state());
  ASSERT_TRUE(main_frame);

  NSTimeInterval js_timeout = kWaitForJSCompletionTimeout;

  __block bool called = false;
  std::vector<base::Value> params;
  main_frame->CallJavaScriptFunction(
      "frameMessaging.getFrameId", params,
      base::BindOnce(^(const base::Value* value) {
        ASSERT_TRUE(value->is_string());
        EXPECT_EQ(value->GetString(), main_frame->GetFrameId());
        called = true;
      }),
      // Increase feature timeout in order to fail on test specific timeout.
      base::TimeDelta::FromSeconds(2 * js_timeout));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(js_timeout, ^bool {
    return called;
  }));
}

TEST_F(WebFrameImplIntTest, CallJavaScriptFunctionOnIframe) {
  if (!base::ios::IsRunningOnIOS11OrLater()) {
    // Frame messaging is not supported on iOS 10.
    return;
  }

  ASSERT_TRUE(LoadHtml("<p><iframe srcdoc='<p>'/>"));

  __block WebFramesManagerImpl* manager =
      WebFramesManagerImpl::FromWebState(web_state());
  ASSERT_TRUE(WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForJSCompletionTimeout, ^bool {
        return manager->GetAllWebFrames().size() == 2;
      }));

  NSTimeInterval js_timeout = kWaitForJSCompletionTimeout;
  WebFrame* iframe = GetChildWebFrameForWebState(web_state());
  ASSERT_TRUE(iframe);

  __block bool called = false;
  std::vector<base::Value> params;
  iframe->CallJavaScriptFunction(
      "frameMessaging.getFrameId", params,
      base::BindOnce(^(const base::Value* value) {
        ASSERT_TRUE(value->is_string());
        EXPECT_EQ(value->GetString(), iframe->GetFrameId());
        called = true;
      }),
      // Increase feature timeout in order to fail on test specific timeout.
      base::TimeDelta::FromSeconds(2 * js_timeout));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(js_timeout, ^bool {
    return called;
  }));
}

TEST_F(WebFrameImplIntTest, CallJavaScriptFunctionTimeout) {
  if (!base::ios::IsRunningOnIOS11OrLater()) {
    // Frame messaging is not supported on iOS 10.
    return;
  }

  // Inject a function which will never return in order to test feature timeout.
  ExecuteJavaScript(
      @"__gCrWeb.testFunctionNeverReturns = function() {"
       "  while(true) {}"
       "};");

  ASSERT_TRUE(LoadHtml("<p>"));

  WebFrame* main_frame = GetMainWebFrameForWebState(web_state());
  ASSERT_TRUE(main_frame);

  __block bool called = false;
  std::vector<base::Value> params;
  main_frame->CallJavaScriptFunction(
      "testFunctionNeverReturns", params,
      base::BindOnce(^(const base::Value* value) {
        EXPECT_FALSE(value);
        called = true;
      }),
      // A small timeout less than kWaitForJSCompletionTimeout. Since this test
      // case tests the timeout, it will take at least this long to execute.
      // This value should be very small to avoid increasing test suite
      // execution time, but long enough to avoid flake.
      base::TimeDelta::FromMilliseconds(100));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
    return called;
  }));
}

// Tests that messages routed through CallJavaScriptFunction cannot be replayed.
TEST_F(WebFrameImplIntTest, PreventMessageReplay) {
  if (!base::ios::IsRunningOnIOS11OrLater()) {
    // Frame messaging is not supported on iOS 10.
    return;
  }

  ASSERT_TRUE(LoadHtml("<p>"));

  WebFrame* main_frame = GetMainWebFrameForWebState(web_state());
  ASSERT_TRUE(main_frame);

  // Inject function into main frame to intercept encrypted message targeted for
  // the iframe.
  ExecuteJavaScript(
      @"var sensitiveValue = 0;"
       "__gCrWeb.frameMessaging.incrementSensitiveValue = function() {"
       "  sensitiveValue = sensitiveValue + 1;"
       "  return sensitiveValue;"
       "};"

       "var originalRouteMessage = __gCrWeb.frameMessaging.routeMessage;"
       "var interceptedMessagePayload = '';"
       "var interceptedMessageIv = '';"
       "var interceptedMessageFrameId = '';"

       "var replayInterceptedMessage = function() {"
       "  originalRouteMessage("
       "    interceptedMessagePayload,"
       "    interceptedMessageIv,"
       "    interceptedMessageFrameId"
       "  );"
       "};"

       "__gCrWeb.frameMessaging.routeMessage ="
       "    function(payload, iv, target_frame_id) {"
       "  interceptedMessagePayload = payload;"
       "  interceptedMessageIv = iv;"
       "  interceptedMessageFrameId = target_frame_id;"
       "  replayInterceptedMessage();"
       "};");

  NSTimeInterval js_timeout = kWaitForJSCompletionTimeout;

  EXPECT_EQ(0, [ExecuteJavaScript(@"sensitiveValue") intValue]);

  __block bool called = false;
  std::vector<base::Value> params;
  main_frame->CallJavaScriptFunction(
      "frameMessaging.incrementSensitiveValue", params,
      base::BindOnce(^(const base::Value* value) {
        ASSERT_TRUE(value->is_double());
        EXPECT_EQ(1, static_cast<int>(value->GetDouble()));
        called = true;
      }),
      // Increase feature timeout in order to fail on test specific timeout.
      base::TimeDelta::FromSeconds(2 * js_timeout));

  EXPECT_TRUE(WaitUntilConditionOrTimeout(js_timeout, ^bool {
    return called;
  }));

  EXPECT_NSEQ(@1, ExecuteJavaScript(@"sensitiveValue"));

  ExecuteJavaScript(@"replayInterceptedMessage()");

  // Value should not increase because replaying message should not re-execute
  // the called function.
  EXPECT_NSEQ(@1, ExecuteJavaScript(@"sensitiveValue"));
}

}  // namespace web
