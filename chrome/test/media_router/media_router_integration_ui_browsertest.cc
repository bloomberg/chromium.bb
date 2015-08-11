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

IN_PROC_BROWSER_TEST_F(MediaRouterIntegrationBrowserTest, MANUAL_Dialog_Basic) {
  OpenTestPage(FILE_PATH_LITERAL("basic_test.html"));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WebContents* dialog_contents = OpenMRDialog(web_contents);

  // Verify the sink list.
  std::string script;
  script = base::StringPrintf(
      "domAutomationController.send(window.document.getElementById("
      "'media-router-container').sinkList.length)");
  ASSERT_EQ(2, ExecuteScriptAndExtractInt(dialog_contents, script));

  ChooseSink(web_contents, "id1", "");
  WaitUntilRouteCreated();

  // Verify the new route.
  script = base::StringPrintf(
      "domAutomationController.send(window.document.getElementById("
      "'media-router-container').routeList[0].title)");
  std::string route_title = ExecuteScriptAndExtractString(
      dialog_contents, script);
  ASSERT_EQ("Test Route", route_title);

  script = base::StringPrintf(
      "domAutomationController.send(window.document.getElementById("
      "'media-router-container').routeList[0].id)");
  std::string route_id = ExecuteScriptAndExtractString(dialog_contents, script);
  std::string current_route = base::StringPrintf(
      "{'id': '%s', 'sinkId': '%s', 'title': '%s', 'isLocal': '%s'}",
      route_id.c_str(), "id1", route_title.c_str(), "false");

  ChooseSink(web_contents, "id1", current_route);
  // TODO(leilei): Verify the router details dialog, including the title and
  // the text, also close the route.
}

}  // namespace media_router
