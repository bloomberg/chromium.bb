// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/vr_browser_test.h"
#include "chrome/browser/vr/test/vr_transition_utils.h"
#include "content/public/test/browser_test_utils.h"

// Browser test equivalent of
// chrome/android/javatests/src/.../browser/vr_shell/WebVrTransitionTest.java.
// End-to-end tests for transitioning between WebVR's magic window and
// presentation modes.

namespace vr {

// Tests that a successful requestPresent call actually enters VR
IN_PROC_BROWSER_TEST_F(VrBrowserTest, REQUIRES_GPU(TestPresentation)) {
  LoadUrlAndAwaitInitialization(
      GetHtmlTestFile("test_requestPresent_enters_vr"));
  enterPresentationAndWait(GetFirstTabWebContents());
  EXPECT_TRUE(RunJavaScriptAndExtractBoolOrFail("vrDisplay.isPresenting",
                                                GetFirstTabWebContents()))
      << "Was not presenting after requestPresent";
  EndTest(GetFirstTabWebContents());
}

}  // namespace vr
