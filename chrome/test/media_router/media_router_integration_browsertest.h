// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_INTEGRATION_BROWSERTEST_H_
#define CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_INTEGRATION_BROWSERTEST_H_

#include <string>

#include "base/debug/stack_trace.h"
#include "base/files/file_path.h"
#include "chrome/browser/ui/toolbar/media_router_action.h"
#include "chrome/test/media_router/media_router_base_browsertest.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"


namespace media_router {

class MediaRouter;

class MediaRouterIntegrationBrowserTest : public MediaRouterBaseBrowserTest {
 public:
  MediaRouterIntegrationBrowserTest();
  ~MediaRouterIntegrationBrowserTest() override;

 protected:
  // InProcessBrowserTest Overrides
  void TearDownOnMainThread() override;

  // Simulate user action to choose one sink in the popup dialog.
  // |web_contents|: The web contents of the test page which invokes the popup
  //                 dialog.
  // |sink_id|: The sink id.
  // |current_route|: The JSON string which represents current route.
  void ChooseSink(content::WebContents* web_contents,
                  const std::string& sink_id, const std::string& current_route);

  // Execute javascript and check the return value.
  static void ExecuteJavaScriptAPI(content::WebContents* web_contents,
                            const std::string& script);

  static int ExecuteScriptAndExtractInt(
      const content::ToRenderFrameHost& adapter,
      const std::string& script);

  static std::string ExecuteScriptAndExtractString(
      const content::ToRenderFrameHost& adapter, const std::string& script);

  // Get the chrome modal dialog.
  // |web_contents|: The web contents of the test page which invokes the popup
  //                 dialog.
  content::WebContents* GetMRDialog(content::WebContents* web_contents);

  void OpenTestPage(base::FilePath::StringPieceType file);
  void OpenTestPageInNewTab(base::FilePath::StringPieceType file);

  void SetTestData(base::FilePath::StringPieceType test_data_file);

  // Start session and wait until the pop dialog shows up.
  // |web_contents|: The web contents of the test page which invokes the popup
  //                 dialog.
  void StartSession(content::WebContents* web_contents);

  // Open the chrome modal dialog.
  // |web_contents|: The web contents of the test page which invokes the popup
  //                 dialog.
  content::WebContents* OpenMRDialog(content::WebContents* web_contents);

  bool IsRouteCreatedOnUI();

  void WaitUntilRouteCreated();

 private:
  // Get the full path of the resource file.
  // |relative_path|: The relative path to
  //                  <chromium src>/out/<build config>/media_router/
  //                  browser_test_resources/
  base::FilePath GetResourceFile(
      base::FilePath::StringPieceType relative_path) const;

  scoped_ptr<content::TestNavigationObserver> test_navigation_observer_;
};

}  // namespace media_router

#endif  // CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_INTEGRATION_BROWSERTEST_H_
