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

// Tests that a successful requestPresent call actually enters VR.
IN_PROC_BROWSER_TEST_F(VrBrowserTestStandard,
                       REQUIRES_GPU(TestRequestPresentEntersVr)) {
  LoadUrlAndAwaitInitialization(
      GetHtmlTestFile("test_requestPresent_enters_vr"));
  EnterPresentationAndWait(GetFirstTabWebContents());
  EXPECT_TRUE(RunJavaScriptAndExtractBoolOrFail("vrDisplay.isPresenting",
                                                GetFirstTabWebContents()))
      << "Was not presenting after requestPresent";
  EndTest(GetFirstTabWebContents());
}

// Tests that window.requestAnimationFrame continues to fire while in WebVR
// presentation since the tab is still visible.
IN_PROC_BROWSER_TEST_F(VrBrowserTestStandard,
                       REQUIRES_GPU(TestWindowRafFiresWhilePresenting)) {
  LoadUrlAndAwaitInitialization(
      GetHtmlTestFile("test_window_raf_fires_while_presenting"));
  ExecuteStepAndWait("stepVerifyBeforePresent()", GetFirstTabWebContents());
  EnterPresentationOrFail(GetFirstTabWebContents());
  ExecuteStepAndWait("stepVerifyDuringPresent()", GetFirstTabWebContents());
  ExitPresentationOrFail(GetFirstTabWebContents());
  ExecuteStepAndWait("stepVerifyAfterPresent()", GetFirstTabWebContents());
  EndTest(GetFirstTabWebContents());
}

// Tests that WebVR is not exposed if the flag is not on and the page does not
// have an origin trial token. Since WebVR isn't actually used, we can remove
// the GPU requirement.
IN_PROC_BROWSER_TEST_F(VrBrowserTestWebVrDisabled,
                       TestWebVrDisabledWithoutFlagSet) {
  LoadUrlAndAwaitInitialization(
      GetHtmlTestFile("test_webvr_disabled_without_flag_set"));
  WaitOnJavaScriptStep(GetFirstTabWebContents());
  EndTest(GetFirstTabWebContents());
}

// Tests that WebVR does not return any devices if OpenVR support is disabled.
// Since OpenVR isn't actually used, we can remove the GPU requirement.
IN_PROC_BROWSER_TEST_F(VrBrowserTestOpenVrDisabled,
                       TestWebVrNoDevicesWithoutOpenVr) {
  LoadUrlAndAwaitInitialization(GetHtmlTestFile("generic_webvr_page"));
  EXPECT_FALSE(VrDisplayFound(GetFirstTabWebContents()));
}

}  // namespace vr
