// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/vr_browser_test.h"
#include "chrome/browser/vr/test/vr_transition_utils.h"

// Browser test equivalent of
// chrome/android/javatests/src/.../browser/vr_shell/WebVrInputTest.java.
// End-to-end tests for user input interaction with WebVR.

namespace vr {

// Test that focus is locked to the presenting display for the purposes of VR
// input.
IN_PROC_BROWSER_TEST_F(VrBrowserTestStandard,
                       REQUIRES_GPU(TestPresentationLocksFocus)) {
  LoadUrlAndAwaitInitialization(
      GetHtmlTestFile("test_presentation_locks_focus"));
  EnterPresentationOrFail(GetFirstTabWebContents());
  ExecuteStepAndWait("stepSetupFocusLoss()", GetFirstTabWebContents());
  EndTest(GetFirstTabWebContents());
}

}  // namespace vr
