// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include "chrome/browser/vr/test/webvr_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"

namespace vr {

bool WebVrBrowserTestBase::XrDeviceFound(content::WebContents* web_contents) {
  return RunJavaScriptAndExtractBoolOrFail("vrDisplay != null", web_contents);
}

void WebVrBrowserTestBase::EnterSessionWithUserGesture(
    content::WebContents* web_contents) {
  // ExecuteScript runs with a user gesture, so we can just directly call
  // requestPresent instead of having to do the hacky workaround the
  // instrumentation tests use of actually sending a click event to the canvas.
  EXPECT_TRUE(content::ExecuteScript(web_contents, "onVrRequestPresent()"));
}

void WebVrBrowserTestBase::EnterSessionWithUserGestureOrFail(
    content::WebContents* web_contents) {
  EnterSessionWithUserGesture(web_contents);
  EXPECT_TRUE(PollJavaScriptBoolean("vrDisplay.isPresenting", kPollTimeoutLong,
                                    web_contents));
}

void WebVrBrowserTestBase::EndSession(content::WebContents* web_contents) {
  EXPECT_TRUE(content::ExecuteScript(web_contents, "vrDisplay.exitPresent()"));
}

void WebVrBrowserTestBase::EndSessionOrFail(
    content::WebContents* web_contents) {
  EndSession(web_contents);
  EXPECT_TRUE(PollJavaScriptBoolean("vrDisplay.isPresenting == false",
                                    kPollTimeoutLong, web_contents));
}

}  // namespace vr
