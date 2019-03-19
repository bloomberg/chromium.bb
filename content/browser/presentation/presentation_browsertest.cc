// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"

namespace content {

using PresentationBrowserTest = ContentBrowserTest;

// Regression test that verifies that calling getAvailability() twice on the
// same PresentationRequest does not return an undefined object.
// TODO(mlamouri,mfoltz), update the test after [SameObject] is used,
// see https://crbug.com/653131
// Broken; see https://crbug.com/940503 .
IN_PROC_BROWSER_TEST_F(PresentationBrowserTest,
                       DISABLED_AvailabilityNotUndefined) {
  GURL test_url = GetTestUrl("", "hello.html");

  TestNavigationObserver navigation_observer(shell()->web_contents(), 1);
  shell()->LoadURL(test_url);

  navigation_observer.Wait();

  ExecuteScriptAndGetValue(shell()->web_contents()->GetMainFrame(),
                           "const r = new PresentationRequest('foo.html')");

  ExecuteScriptAndGetValue(shell()->web_contents()->GetMainFrame(),
                           "const p1 = r.getAvailability();");

  ExecuteScriptAndGetValue(shell()->web_contents()->GetMainFrame(),
                           "const p2 = r.getAvailability()");

  bool is_p1_undefined =
      ExecuteScriptAndGetValue(shell()->web_contents()->GetMainFrame(),
                               "p1 === undefined")
          .GetBool();

  bool is_p2_undefined =
      ExecuteScriptAndGetValue(shell()->web_contents()->GetMainFrame(),
                               "p2 === undefined")
          .GetBool();

  EXPECT_FALSE(is_p1_undefined);
  EXPECT_FALSE(is_p2_undefined);
}

}  // namespace content
