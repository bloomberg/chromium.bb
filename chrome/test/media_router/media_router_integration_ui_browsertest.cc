// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/media_router_integration_browsertest.h"

#include "base/files/file_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"


namespace media_router {

namespace {
const char kTestSinkName[] = "test-sink-1";
}

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest, MANUAL_Dialog_Basic) {
  OpenTestPage(FILE_PATH_LITERAL("basic_test.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WebContents* dialog_contents = OpenMRDialog(web_contents);

  // Verify the sink list.
  std::string sink_length_script = base::StringPrintf(
      "domAutomationController.send("
      "window.document.getElementById('media-router-container')."
      "sinkList.length)");
  ASSERT_EQ(2, ExecuteScriptAndExtractInt(dialog_contents, sink_length_script));

  ChooseSink(web_contents, kTestSinkName);
  WaitUntilRouteCreated();

  // Verify the route details page.
  std::string route_info_script = base::StringPrintf(
      "domAutomationController.send("
      "window.document.getElementById('media-router-container').shadowRoot."
      "getElementById('route-details').shadowRoot.getElementById("
      "'route-information').getElementsByTagName('span')[0].innerText)");
  std::string route_information = ExecuteScriptAndExtractString(
      dialog_contents, route_info_script);
  ASSERT_EQ("Casting: Test Route", route_information);

  std::string sink_name_script = base::StringPrintf(
      "domAutomationController.send("
      "window.document.getElementById('media-router-container').shadowRoot."
      "getElementById('route-details').shadowRoot.getElementById('sink-name')."
      "innerText)");
  std::string sink_name = ExecuteScriptAndExtractString(
      dialog_contents, sink_name_script);
  ASSERT_EQ(kTestSinkName, sink_name);

  // Close route.
  CloseRouteOnUI();
}

}  // namespace media_router
