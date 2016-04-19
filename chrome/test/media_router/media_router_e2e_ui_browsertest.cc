// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/media_router_e2e_browsertest.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

IN_PROC_BROWSER_TEST_F(MediaRouterE2EBrowserTest, MANUAL_MirrorHTML5Video) {
  OpenMediaPage();
  content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

  // Play the video on loop.
  std::string script = "document.getElementsByTagName('video')[0].loop=true;";
  ExecuteScript(web_contents, script);
  content::WebContents* dialog_contents = OpenMRDialog(web_contents);
  ASSERT_TRUE(dialog_contents);
  // Wait for 1s to make sure the dialog finishes rendering.
  Wait(base::TimeDelta::FromSeconds(1));
  WaitUntilSinkDiscoveredOnUI();
  ChooseSink(web_contents, receiver());
  WaitUntilRouteCreated();

  // Mirror tab for 10s.
  Wait(base::TimeDelta::FromSeconds(10));

  // Go to full screen.
  script = "document.getElementsByTagName('video')[0]."
      "webkitRequestFullScreen();";
  ExecuteScript(web_contents, script);
  OpenMRDialog(web_contents);

  // Check the mirroring session is still live.
  ASSERT_TRUE(!GetRouteId(receiver()).empty());
  Wait(base::TimeDelta::FromSeconds(20));
  CloseRouteOnUI();
}

}  // namespace media_router
