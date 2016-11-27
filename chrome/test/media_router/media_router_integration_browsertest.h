// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_INTEGRATION_BROWSERTEST_H_
#define CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_INTEGRATION_BROWSERTEST_H_

#include <memory>
#include <string>

#include "base/debug/stack_trace.h"
#include "base/files/file_path.h"
#include "chrome/browser/ui/toolbar/media_router_action.h"
#include "chrome/test/media_router/media_router_base_browsertest.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"


namespace media_router {

class MediaRouterIntegrationBrowserTest : public MediaRouterBaseBrowserTest {
 public:
  MediaRouterIntegrationBrowserTest();
  ~MediaRouterIntegrationBrowserTest() override;

 protected:
  // InProcessBrowserTest Overrides
  void TearDownOnMainThread() override;

  // MediaRouterBaseBrowserTest Overrides
  void ParseCommandLine() override;

  // Simulate user action to choose one sink in the popup dialog.
  // |web_contents|: The web contents of the test page which invokes the popup
  //                 dialog.
  // |sink_name|: The sink's human readable name.
  void ChooseSink(content::WebContents* web_contents,
                  const std::string& sink_name);

  // Checks that the request initiated from |web_contents| to start session
  // failed with expected |error_name| and |error_message_substring|.
  void CheckStartFailed(content::WebContents* web_contents,
                        const std::string& error_name,
                        const std::string& error_message_substring);

  // Execute javascript and check the return value.
  static void ExecuteJavaScriptAPI(content::WebContents* web_contents,
                            const std::string& script);

  static int ExecuteScriptAndExtractInt(
      const content::ToRenderFrameHost& adapter,
      const std::string& script);

  static std::string ExecuteScriptAndExtractString(
      const content::ToRenderFrameHost& adapter, const std::string& script);

  void ClickDialog();

  static bool ExecuteScriptAndExtractBool(
      const content::ToRenderFrameHost& adapter,
      const std::string& script);

  static void ExecuteScript(const content::ToRenderFrameHost& adapter,
                            const std::string& script);

  // Get the chrome modal dialog.
  // |web_contents|: The web contents of the test page which invokes the popup
  //                 dialog.
  content::WebContents* GetMRDialog(content::WebContents* web_contents);

  // Checks that the chrome modal dialog does not exist.
  bool IsDialogClosed(content::WebContents* web_contents);
  void WaitUntilDialogClosed(content::WebContents* web_contents);

  void CheckDialogRemainsOpen(content::WebContents* web_contents);

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

  bool IsRouteClosedOnUI();

  bool IsSinkDiscoveredOnUI();

  // Close route through clicking 'Stop casting' button in route details dialog.
  void CloseRouteOnUI();

  // Wait for the route to show up in the UI with a timeout. Fails if the
  // route did not show up before the timeout.
  void WaitUntilRouteCreated();

  // Wait until there is an issue showing in the UI.
  void WaitUntilIssue();

  // Returns true if there is an issue showing in the UI.
  bool IsUIShowingIssue();

  // Returns the title of issue showing in UI. It is an error to call this if
  // there are no issues showing in UI.
  std::string GetIssueTitle();

  // Returns the route ID for the specific sink.
  std::string GetRouteId(const std::string& sink_id);

  // Wait for the specific sink shows up in UI with a timeout. Fails if the sink
  // doesn't show up before the timeout.
  void WaitUntilSinkDiscoveredOnUI();

  // Checks if media router dialog is fully loaded.
  bool IsDialogLoaded(content::WebContents* dialog_contents);

  // Wait until media router dialog is fully loaded.
  void WaitUntilDialogFullyLoaded(content::WebContents* dialog_contents);

  std::string receiver() const { return receiver_; }

 private:
  // Get the full path of the resource file.
  // |relative_path|: The relative path to
  //                  <chromium src>/out/<build config>/media_router/
  //                  browser_test_resources/
  base::FilePath GetResourceFile(
      base::FilePath::StringPieceType relative_path) const;

  std::unique_ptr<content::TestNavigationObserver> test_navigation_observer_;

  // Fields
  std::string receiver_;
};

}  // namespace media_router

#endif  // CHROME_TEST_MEDIA_ROUTER_MEDIA_ROUTER_INTEGRATION_BROWSERTEST_H_
