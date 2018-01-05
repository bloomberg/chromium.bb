// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/vr_transition_utils.h"
#include "chrome/browser/vr/test/vr_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"

namespace vr {

void EnterPresentation(content::WebContents* web_contents) {
  // ExecuteScript runs with a user gesture, so we can just directly call
  // requestPresent instead of having to do the hacky workaround the
  // instrumentation tests use of actually sending a click event to the canvas.
  EXPECT_TRUE(content::ExecuteScript(web_contents, "onVrRequestPresent()"));
}

void EnterPresentationAndWait(content::WebContents* web_contents) {
  EnterPresentation(web_contents);
  VrBrowserTestBase::WaitOnJavaScriptStep(web_contents);
}

void EnterPresentationOrFail(content::WebContents* web_contents) {
  EnterPresentation(web_contents);
  EXPECT_TRUE(VrBrowserTestBase::PollJavaScriptBoolean(
      "vrDisplay.isPresenting", VrBrowserTestBase::kPollTimeoutLong,
      web_contents));
}

void ExitPresentation(content::WebContents* web_contents) {
  EXPECT_TRUE(content::ExecuteScript(web_contents, "vrDisplay.exitPresent()"));
}

void ExitPresentationAndWait(content::WebContents* web_contents) {
  ExitPresentation(web_contents);
  VrBrowserTestBase::WaitOnJavaScriptStep(web_contents);
}

void ExitPresentationOrFail(content::WebContents* web_contents) {
  ExitPresentation(web_contents);
  EXPECT_TRUE(VrBrowserTestBase::PollJavaScriptBoolean(
      "vrDisplay.isPresenting == false", VrBrowserTestBase::kPollTimeoutLong,
      web_contents));
}

}  // namespace vr
