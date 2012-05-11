// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/stringprintf.h"
#include "chrome/browser/extensions/extension_devtools_browsertest.h"
#include "chrome/browser/extensions/extension_error_reporter.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/devtools_agent_host_registry.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_util.h"

using content::DevToolsAgentHost;
using content::DevToolsAgentHostRegistry;
using content::DevToolsClientHost;
using content::DevToolsManager;
using content::WebContents;

// Looks for an background ExtensionHost whose URL has the given path component
// (including leading slash).  Also verifies that the expected number of hosts
// are loaded.
static ExtensionHost* FindBackgroundHostWithPath(
    ExtensionProcessManager* manager,
    const std::string& path,
    int expected_hosts) {
  ExtensionHost* host = NULL;
  int num_hosts = 0;
  ExtensionProcessManager::ExtensionHostSet background_hosts =
      manager->background_hosts();
  for (ExtensionProcessManager::const_iterator iter = background_hosts.begin();
       iter != background_hosts.end(); ++iter) {
    if ((*iter)->GetURL().path() == path) {
      EXPECT_FALSE(host);
      host = *iter;
    }
    num_hosts++;
  }
  EXPECT_EQ(expected_hosts, num_hosts);
  EXPECT_TRUE(host);
  return host;
}

// Tests for the experimental timeline extensions API.
IN_PROC_BROWSER_TEST_F(ExtensionDevToolsBrowserTest, TimelineApi) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("devtools").AppendASCII("timeline_api")));

  // Get the ExtensionHost that is hosting our background page.
  ExtensionProcessManager* manager =
      browser()->profile()->GetExtensionProcessManager();
  ExtensionHost* host = FindBackgroundHostWithPath(manager,
                                                   "/background.html", 1);

  // Grab a handle to the DevToolsManager so we can forward messages to it.
  DevToolsManager* devtools_manager = DevToolsManager::GetInstance();

  // Grab the tab_id of whatever tab happens to be first.
  WebContents* web_contents = browser()->GetWebContentsAt(0);
  ASSERT_TRUE(web_contents);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  // Test setup.
  bool result = false;
  std::wstring register_listeners_js = base::StringPrintf(
      L"setListenersOnTab(%d)", tab_id);
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", register_listeners_js, &result));
  EXPECT_TRUE(result);

  // Setting the events should have caused an ExtensionDevToolsBridge to be
  // registered for the tab's RenderViewHost.
  DevToolsAgentHost* agent = DevToolsAgentHostRegistry::GetDevToolsAgentHost(
      web_contents->GetRenderViewHost());
  DevToolsClientHost* devtools_client_host =
      devtools_manager->GetDevToolsClientHostFor(agent);
  ASSERT_TRUE(devtools_client_host);

  // Test onPageEvent event.
  result = false;

  devtools_client_host->DispatchOnInspectorFrontend("");
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testReceivePageEvent()", &result));
  EXPECT_TRUE(result);

  // Test onTabClose event.
  result = false;
  devtools_manager->UnregisterDevToolsClientHostFor(
      DevToolsAgentHostRegistry::GetDevToolsAgentHost(
          web_contents->GetRenderViewHost()));
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host->render_view_host(), L"", L"testReceiveTabCloseEvent()", &result));
  EXPECT_TRUE(result);
}


// Tests that ref counting of listeners from multiple processes works.
IN_PROC_BROWSER_TEST_F(ExtensionDevToolsBrowserTest, ProcessRefCounting) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("devtools").AppendASCII("timeline_api")));

  // Get the ExtensionHost that is hosting our background page.
  ExtensionProcessManager* manager =
      browser()->profile()->GetExtensionProcessManager();
  ExtensionHost* host_one = FindBackgroundHostWithPath(manager,
                                                       "/background.html", 1);

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("devtools").AppendASCII("timeline_api_two")));
  ExtensionHost* host_two = FindBackgroundHostWithPath(manager,
                                                       "/background_two.html",
                                                       2);

  DevToolsManager* devtools_manager = DevToolsManager::GetInstance();

  // Grab the tab_id of whatever tab happens to be first.
  WebContents* web_contents = browser()->GetWebContentsAt(0);
  ASSERT_TRUE(web_contents);
  int tab_id = ExtensionTabUtil::GetTabId(web_contents);

  // Test setup.
  bool result = false;
  std::wstring register_listeners_js = base::StringPrintf(
      L"setListenersOnTab(%d)", tab_id);
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host_one->render_view_host(), L"", register_listeners_js, &result));
  EXPECT_TRUE(result);

  // Setting the event listeners should have caused an ExtensionDevToolsBridge
  // to be registered for the tab's RenderViewHost.
  ASSERT_TRUE(devtools_manager->GetDevToolsClientHostFor(
      DevToolsAgentHostRegistry::GetDevToolsAgentHost(
          web_contents->GetRenderViewHost())));

  // Register listeners from the second extension as well.
  std::wstring script = base::StringPrintf(L"registerListenersForTab(%d)",
                                           tab_id);
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host_two->render_view_host(), L"", script, &result));
  EXPECT_TRUE(result);

  // Removing the listeners from the first extension should leave the bridge
  // alive.
  result = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host_one->render_view_host(), L"", L"unregisterListeners()", &result));
  EXPECT_TRUE(result);
  ASSERT_TRUE(devtools_manager->GetDevToolsClientHostFor(
      DevToolsAgentHostRegistry::GetDevToolsAgentHost(
          web_contents->GetRenderViewHost())));

  // Removing the listeners from the second extension should tear the bridge
  // down.
  result = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      host_two->render_view_host(), L"", L"unregisterListeners()", &result));
  EXPECT_TRUE(result);
  ASSERT_FALSE(devtools_manager->GetDevToolsClientHostFor(
      DevToolsAgentHostRegistry::GetDevToolsAgentHost(
          web_contents->GetRenderViewHost())));
}
