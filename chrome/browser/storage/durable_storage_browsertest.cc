// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/dom_operation_notification_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class DurableStorageBrowserTest : public InProcessBrowserTest {
 public:
  DurableStorageBrowserTest();
  ~DurableStorageBrowserTest() override;

  void SetUpCommandLine(base::CommandLine*) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DurableStorageBrowserTest);
};

DurableStorageBrowserTest::DurableStorageBrowserTest() {}

DurableStorageBrowserTest::~DurableStorageBrowserTest() {}

void DurableStorageBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  command_line->AppendSwitch(
              switches::kEnableExperimentalWebPlatformFeatures);
}

IN_PROC_BROWSER_TEST_F(DurableStorageBrowserTest, FirstTabSeesResult) {
  EXPECT_TRUE(embedded_test_server()->Started() ||
              embedded_test_server()->InitializeAndWaitUntilReady());

  Browser* current_browser = browser();

  const GURL& url =
      embedded_test_server()->GetURL("/durable/durability-window1.html");
  ui_test_utils::NavigateToURL(current_browser, url);
  content::RenderFrameHost* render_frame_host =
      current_browser->tab_strip_model()
          ->GetActiveWebContents()
          ->GetMainFrame();
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      render_frame_host, "checkPermission()", &result));
  EXPECT_EQ("default", result);

  chrome::NewTab(current_browser);
  const GURL& url2 =
      embedded_test_server()->GetURL("/durable/durability-window2.html");
  ui_test_utils::NavigateToURL(current_browser, url2);
  render_frame_host = current_browser->tab_strip_model()
                          ->GetActiveWebContents()
                          ->GetMainFrame();
  PermissionBubbleManager::FromWebContents(
      current_browser->tab_strip_model()->GetActiveWebContents())
      ->set_auto_response_for_test(PermissionBubbleManager::ACCEPT_ALL);
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      render_frame_host, "requestPermission()", &result));
  EXPECT_EQ("granted", result);

  current_browser->tab_strip_model()->ActivateTabAt(0, false);
  render_frame_host = current_browser->tab_strip_model()
                          ->GetActiveWebContents()
                          ->GetMainFrame();
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      render_frame_host, "checkPermission()", &result));
  EXPECT_EQ("granted", result);
}
