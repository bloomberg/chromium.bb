// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state/web_frame.h"
#import "ios/web/public/web_state/web_frame_util.h"
#import "ios/web/public/web_state/web_frames_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Name of JavaScript handler invoked.
const char kFindInPageFindString[] = "findInPage.findString";

// Find strings.
const char kFindStringFoo[] = "foo";
const char kFindString12345[] = "12345";

// Pump search timeout in milliseconds.
const double kPumpSearchTimeout = 100.0;

// Timeout for frame javascript execution, in seconds.
const double kCallJavascriptFunctionTimeout = 3.0;
}

namespace web {

// Calls FindInPage Javascript handlers and checks that return values are
// correct.
class FindInPageWebJsTest : public WebTestWithWebState {
 protected:
  // Returns WebFramesManager instance.
  WebFramesManager* frames_manager() {
    return WebFramesManager::FromWebState(web_state());
  }
  // Returns main frame for |web_state_|.
  WebFrame* main_web_frame() { return GetMainWebFrame(web_state()); }
};

// Tests that FindInPage searches in main frame containing a match and responds
// with 1 match.
TEST_F(FindInPageWebJsTest, FindText) {
  ASSERT_TRUE(LoadHtml("<span>foo</span>"));
  WaitForCondition(^{
    return frames_manager()->GetAllWebFrames().size() == 1;
  });

  __block bool message_received = false;
  std::vector<base::Value> params;
  params.push_back(base::Value(kFindStringFoo));
  params.push_back(base::Value(kPumpSearchTimeout));
  main_web_frame()->CallJavaScriptFunction(
      kFindInPageFindString, params, base::BindOnce(^(const base::Value* res) {
        ASSERT_TRUE(res);
        ASSERT_TRUE(res->is_double());
        int count = static_cast<int>(res->GetDouble());
        ASSERT_TRUE(count == 1);
        message_received = true;
      }),
      base::TimeDelta::FromSeconds(kCallJavascriptFunctionTimeout));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return message_received;
  }));
}

// Tests that FindInPage searches in main frame for text that exists but is
// hidden and responds with 0 matches.
TEST_F(FindInPageWebJsTest, FindTextNoResults) {
  ASSERT_TRUE(LoadHtml("<span style='display:none'>foo</span>"));
  WaitForCondition(^{
    return frames_manager()->GetAllWebFrames().size() == 1;
  });
  __block bool message_received = false;
  std::vector<base::Value> params;
  params.push_back(base::Value(kFindStringFoo));
  params.push_back(base::Value(kPumpSearchTimeout));
  main_web_frame()->CallJavaScriptFunction(
      kFindInPageFindString, params, base::BindOnce(^(const base::Value* res) {
        ASSERT_TRUE(res);
        ASSERT_TRUE(res->is_double());
        int count = static_cast<int>(res->GetDouble());
        ASSERT_TRUE(count == 0);
        message_received = true;
      }),
      base::TimeDelta::FromSeconds(kCallJavascriptFunctionTimeout));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return message_received;
  }));
}

// Tests that FindInPage searches in child iframe and asserts that a result was
// found.
TEST_F(FindInPageWebJsTest, FindIFrameText) {
  ASSERT_TRUE(LoadHtml(
      "<iframe srcdoc='<html><body><span>foo</span></body></html>'></iframe>"));
  WaitForCondition(^{
    return frames_manager()->GetAllWebFrames().size() == 2;
  });
  std::set<WebFrame*> all_frames = frames_manager()->GetAllWebFrames();
  __block bool message_received = false;
  WebFrame* child_frame = nullptr;
  for (auto* frame : all_frames) {
    if (frame->IsMainFrame()) {
      continue;
    }
    child_frame = frame;
  }
  ASSERT_TRUE(child_frame);
  std::vector<base::Value> params;
  params.push_back(base::Value(kFindStringFoo));
  params.push_back(base::Value(kPumpSearchTimeout));
  child_frame->CallJavaScriptFunction(
      kFindInPageFindString, params, base::BindOnce(^(const base::Value* res) {
        ASSERT_TRUE(res);
        ASSERT_TRUE(res->is_double());
        int count = static_cast<int>(res->GetDouble());
        ASSERT_TRUE(count == 1);
        message_received = true;
      }),
      base::TimeDelta::FromSeconds(kCallJavascriptFunctionTimeout));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return message_received;
  }));
}

// Tests that FindInPage works when searching for white space.
TEST_F(FindInPageWebJsTest, FindWhiteSpace) {
  ASSERT_TRUE(LoadHtml("<span> </span>"));
  WaitForCondition(^{
    return frames_manager()->GetAllWebFrames().size() == 1;
  });
  __block bool message_received = false;
  std::vector<base::Value> params;
  params.push_back(base::Value(" "));
  params.push_back(base::Value(kPumpSearchTimeout));
  main_web_frame()->CallJavaScriptFunction(
      kFindInPageFindString, params, base::BindOnce(^(const base::Value* res) {
        ASSERT_TRUE(res);
        ASSERT_TRUE(res->is_double());
        int count = static_cast<int>(res->GetDouble());
        ASSERT_TRUE(count == 1);
        message_received = true;
      }),
      base::TimeDelta::FromSeconds(kCallJavascriptFunctionTimeout));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return message_received;
  }));
}

// Tests that FindInPage works when match results cover multiple HTML Nodes.
TEST_F(FindInPageWebJsTest, FindAcrossMultipleNodes) {
  ASSERT_TRUE(
      LoadHtml("<p>xx1<span>2</span>3<a>4512345xxx12</a>34<a>5xxx12345xx</p>"));
  WaitForCondition(^{
    return frames_manager()->GetAllWebFrames().size() == 1;
  });
  __block bool message_received = false;
  std::vector<base::Value> params;
  params.push_back(base::Value(kFindString12345));
  params.push_back(base::Value(kPumpSearchTimeout));
  main_web_frame()->CallJavaScriptFunction(
      kFindInPageFindString, params, base::BindOnce(^(const base::Value* res) {
        ASSERT_TRUE(res);
        ASSERT_TRUE(res->is_double());
        int count = static_cast<int>(res->GetDouble());
        ASSERT_TRUE(count == 4);
        message_received = true;
      }),
      base::TimeDelta::FromSeconds(kCallJavascriptFunctionTimeout));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^{
    return message_received;
  }));
}

}  // namespace web
