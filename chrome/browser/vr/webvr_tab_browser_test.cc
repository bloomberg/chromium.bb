// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/vr/test/vr_browser_test.h"
#include "chrome/browser/vr/test/vr_transition_utils.h"
#include "url/gurl.h"
#include "url/url_constants.h"

// Browser test equivalent of
// chrome/android/javatests/src/.../browser/vr_shell/WebVrTabTest.java.
// End-to-end tests for testing WebVR's interaction with multiple tabss.

namespace vr {

// Tests that non-focused tabs cannot get pose information
IN_PROC_BROWSER_TEST_F(VrBrowserTestStandard,
                       REQUIRES_GPU(TestPoseDataUnfocusedTab)) {
  LoadUrlAndAwaitInitialization(
      GetHtmlTestFile("test_pose_data_unfocused_tab"));
  ExecuteStepAndWait("stepCheckFrameDataWhileFocusedTab()",
                     GetFirstTabWebContents());
  chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL),
                   -1 /* index, append to end */, true /* foreground */);
  ExecuteStepAndWait("stepCheckFrameDataWhileNonFocusedTab()",
                     GetFirstTabWebContents());
  EndTest(GetFirstTabWebContents());
}

}  // namespace vr
