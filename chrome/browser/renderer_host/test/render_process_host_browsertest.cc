// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"

class RenderProcessHostTest : public InProcessBrowserTest {
 public:
  RenderProcessHostTest() {
    EnableDOMAutomation();
  }

  int RenderProcessHostCount() {
    RenderProcessHost::iterator hosts = RenderProcessHost::AllHostsIterator();
    int count = 0;
    while (!hosts.IsAtEnd()) {
      if (hosts.GetCurrentValue()->HasConnection())
        count++;
      hosts.Advance();
    }
    return count;
  }

  RenderViewHost* FindFirstDevToolsHost() {
    RenderProcessHost::iterator hosts = RenderProcessHost::AllHostsIterator();
    for (; !hosts.IsAtEnd(); hosts.Advance()) {
      RenderProcessHost* render_process_host = hosts.GetCurrentValue();
      DCHECK(render_process_host);
      if (!render_process_host->HasConnection())
        continue;
      RenderProcessHost::listeners_iterator iter(
          render_process_host->ListenersIterator());
      for (; !iter.IsAtEnd(); iter.Advance()) {
        const RenderWidgetHost* widget =
            static_cast<const RenderWidgetHost*>(iter.GetCurrentValue());
        DCHECK(widget);
        if (!widget || !widget->IsRenderView())
          continue;
        RenderViewHost* host = const_cast<RenderViewHost*>(
            static_cast<const RenderViewHost*>(widget));
        RenderViewHostDelegate* host_delegate = host->delegate();
        GURL url = host_delegate->GetURL();
        if (url.SchemeIs(chrome::kChromeDevToolsScheme))
          return host;
      }
    }
    return NULL;
  }
};

IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, ProcessPerTab) {
  // Set max renderers to 1 to force running out of processes.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  parsed_command_line.AppendSwitch(switches::kProcessPerTab);

  int tab_count = 1;
  int host_count = 1;

  // Change the first tab to be the new tab page (TYPE_DOMUI).
  GURL newtab(chrome::kChromeUINewTabURL);
  ui_test_utils::NavigateToURL(browser(), newtab);
  EXPECT_EQ(tab_count, browser()->tab_count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Create a new TYPE_NORMAL tab.  It should be in its own process.
  GURL page1("data:text/html,hello world1");
  browser()->ShowSingletonTab(page1, false);
  if (browser()->tab_count() == tab_count)
    ui_test_utils::WaitForNewTab(browser());
  tab_count++;
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Create another TYPE_NORMAL tab.  It should share the previous process.
  GURL page2("data:text/html,hello world2");
  browser()->ShowSingletonTab(page2, false);
  if (browser()->tab_count() == tab_count)
    ui_test_utils::WaitForNewTab(browser());
  tab_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Create another new tab.  It should share the process with the other WebUI.
  browser()->NewTab();
  if (browser()->tab_count() == tab_count)
    ui_test_utils::WaitForNewTab(browser());
  tab_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Create another new tab.  It should share the process with the other WebUI.
  browser()->NewTab();
  if (browser()->tab_count() == tab_count)
    ui_test_utils::WaitForNewTab(browser());
  tab_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  EXPECT_EQ(host_count, RenderProcessHostCount());
}

// Ensure that DevTools opened to debug DevTools is launched in a separate
// process when --process-per-tab is set. See crbug.com/69873.
IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, DevToolsOnSelfInOwnProcessPPT) {
  CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  parsed_command_line.AppendSwitch(switches::kProcessPerTab);

  int tab_count = 1;
  int host_count = 1;

  GURL page1("data:text/html,hello world1");
  browser()->ShowSingletonTab(page1, false);
  if (browser()->tab_count() == tab_count)
    ui_test_utils::WaitForNewTab(browser());
  tab_count++;
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // DevTools start in docked mode (no new tab), in a separate process.
  browser()->ToggleDevToolsWindow(DEVTOOLS_TOGGLE_ACTION_INSPECT);
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  RenderViewHost* devtools = FindFirstDevToolsHost();
  DCHECK(devtools);

  // DevTools start in a separate process.
  DevToolsManager::GetInstance()->ToggleDevToolsWindow(
      devtools, DEVTOOLS_TOGGLE_ACTION_INSPECT);
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  EXPECT_EQ(host_count, RenderProcessHostCount());
}

// Ensure that DevTools opened to debug DevTools is launched in a separate
// process. See crbug.com/69873.
IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, DevToolsOnSelfInOwnProcess) {
  int tab_count = 1;
  int host_count = 1;

  GURL page1("data:text/html,hello world1");
  browser()->ShowSingletonTab(page1, false);
  if (browser()->tab_count() == tab_count)
    ui_test_utils::WaitForNewTab(browser());
  tab_count++;
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // DevTools start in docked mode (no new tab), in a separate process.
  browser()->ToggleDevToolsWindow(DEVTOOLS_TOGGLE_ACTION_INSPECT);
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  RenderViewHost* devtools = FindFirstDevToolsHost();
  DCHECK(devtools);

  // DevTools start in a separate process.
  DevToolsManager::GetInstance()->ToggleDevToolsWindow(
      devtools, DEVTOOLS_TOGGLE_ACTION_INSPECT);
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  EXPECT_EQ(host_count, RenderProcessHostCount());
}

// When we hit the max number of renderers, verify that the way we do process
// sharing behaves correctly.  In particular, this test is verifying that even
// when we hit the max process limit, that renderers of each type will wind up
// in a process of that type, even if that means creating a new process.
// TODO(erikkay) crbug.com/43448 - disabled for now until we can get a
// reasonable implementation in place.
IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, DISABLED_ProcessOverflow) {
  // Set max renderers to 1 to force running out of processes.
  RenderProcessHost::SetMaxRendererProcessCount(1);

  int tab_count = 1;
  int host_count = 1;
  TabContents* tab1 = NULL;
  TabContents* tab2 = NULL;
  RenderProcessHost* rph1 = NULL;
  RenderProcessHost* rph2 = NULL;
  RenderProcessHost* rph3 = NULL;

  // Change the first tab to be the new tab page (TYPE_DOMUI).
  GURL newtab(chrome::kChromeUINewTabURL);
  ui_test_utils::NavigateToURL(browser(), newtab);
  EXPECT_EQ(tab_count, browser()->tab_count());
  tab1 = browser()->GetTabContentsAt(tab_count - 1);
  rph1 = tab1->GetRenderProcessHost();
  EXPECT_EQ(tab1->GetURL(), newtab);
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Create a new TYPE_NORMAL tab.  It should be in its own process.
  GURL page1("data:text/html,hello world1");
  browser()->ShowSingletonTab(page1, false);
  if (browser()->tab_count() == tab_count)
    ui_test_utils::WaitForNewTab(browser());
  tab_count++;
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  tab1 = browser()->GetTabContentsAt(tab_count - 1);
  rph2 = tab1->GetRenderProcessHost();
  EXPECT_EQ(tab1->GetURL(), page1);
  EXPECT_EQ(host_count, RenderProcessHostCount());
  EXPECT_NE(rph1, rph2);

  // Create another TYPE_NORMAL tab.  It should share the previous process.
  GURL page2("data:text/html,hello world2");
  browser()->ShowSingletonTab(page2, false);
  if (browser()->tab_count() == tab_count)
    ui_test_utils::WaitForNewTab(browser());
  tab_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  tab2 = browser()->GetTabContentsAt(tab_count - 1);
  EXPECT_EQ(tab2->GetURL(), page2);
  EXPECT_EQ(host_count, RenderProcessHostCount());
  EXPECT_EQ(tab2->GetRenderProcessHost(), rph2);

  // Create another TYPE_DOMUI tab.  It should share the process with newtab.
  // Note: intentionally create this tab after the TYPE_NORMAL tabs to exercise
  // bug 43448 where extension and WebUI tabs could get combined into normal
  // renderers.
  GURL history(chrome::kChromeUIHistoryURL);
  browser()->ShowSingletonTab(history, false);
  if (browser()->tab_count() == tab_count)
    ui_test_utils::WaitForNewTab(browser());
  tab_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  tab2 = browser()->GetTabContentsAt(tab_count - 1);
  EXPECT_EQ(tab2->GetURL(), history);
  EXPECT_EQ(host_count, RenderProcessHostCount());
  EXPECT_EQ(tab2->GetRenderProcessHost(), rph1);

  // Create a TYPE_EXTENSION tab.  It should be in its own process.
  // (the bookmark manager is implemented as an extension)
  GURL bookmarks(chrome::kChromeUIBookmarksURL);
  browser()->ShowSingletonTab(bookmarks, false);
  if (browser()->tab_count() == tab_count)
    ui_test_utils::WaitForNewTab(browser());
  tab_count++;
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  tab1 = browser()->GetTabContentsAt(tab_count - 1);
  rph3 = tab1->GetRenderProcessHost();
  EXPECT_EQ(tab1->GetURL(), bookmarks);
  EXPECT_EQ(host_count, RenderProcessHostCount());
  EXPECT_NE(rph1, rph3);
  EXPECT_NE(rph2, rph3);
}
