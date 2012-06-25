// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

using content::RenderViewHost;
using content::RenderWidgetHost;
using content::WebContents;

namespace {

int RenderProcessHostCount() {
  content::RenderProcessHost::iterator hosts =
      content::RenderProcessHost::AllHostsIterator();
  int count = 0;
  while (!hosts.IsAtEnd()) {
    if (hosts.GetCurrentValue()->HasConnection())
      count++;
    hosts.Advance();
  }
  return count;
}

RenderViewHost* FindFirstDevToolsHost() {
  content::RenderProcessHost::iterator hosts =
      content::RenderProcessHost::AllHostsIterator();
  for (; !hosts.IsAtEnd(); hosts.Advance()) {
    content::RenderProcessHost* render_process_host = hosts.GetCurrentValue();
    DCHECK(render_process_host);
    if (!render_process_host->HasConnection())
      continue;
    content::RenderProcessHost::RenderWidgetHostsIterator iter(
        render_process_host->GetRenderWidgetHostsIterator());
    for (; !iter.IsAtEnd(); iter.Advance()) {
      const RenderWidgetHost* widget = iter.GetCurrentValue();
      DCHECK(widget);
      if (!widget || !widget->IsRenderView())
        continue;
      RenderViewHost* host =
          RenderViewHost::From(const_cast<RenderWidgetHost*>(widget));
      WebContents* contents = WebContents::FromRenderViewHost(host);
      GURL url = contents->GetURL();
      if (url.SchemeIs(chrome::kChromeDevToolsScheme))
        return host;
    }
  }
  return NULL;
}

}  // namespace

typedef InProcessBrowserTest ChromeRenderProcessHostTest;

// Ensure that DevTools opened to debug DevTools is launched in a separate
// process when --process-per-tab is set. See crbug.com/69873.
IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostTest,
                       DevToolsOnSelfInOwnProcessPPT) {
  CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  parsed_command_line.AppendSwitch(switches::kProcessPerTab);

  int tab_count = 1;
  int host_count = 1;

  GURL page1("data:text/html,hello world1");
  chrome::ShowSingletonTab(browser(), page1);
  if (browser()->tab_count() == tab_count)
    ui_test_utils::WaitForNewTab(browser());
  tab_count++;
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // DevTools start in docked mode (no new tab), in a separate process.
  chrome::ToggleDevToolsWindow(browser(), DEVTOOLS_TOGGLE_ACTION_INSPECT);
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  RenderViewHost* devtools = FindFirstDevToolsHost();
  DCHECK(devtools);

  // DevTools start in a separate process.
  DevToolsWindow::ToggleDevToolsWindow(
      devtools, DEVTOOLS_TOGGLE_ACTION_INSPECT);
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  EXPECT_EQ(host_count, RenderProcessHostCount());
}

// Ensure that DevTools opened to debug DevTools is launched in a separate
// process. See crbug.com/69873.
IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostTest,
                       DevToolsOnSelfInOwnProcess) {
  int tab_count = 1;
  int host_count = 1;

  GURL page1("data:text/html,hello world1");
  chrome::ShowSingletonTab(browser(), page1);
  if (browser()->tab_count() == tab_count)
    ui_test_utils::WaitForNewTab(browser());
  tab_count++;
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // DevTools start in docked mode (no new tab), in a separate process.
  chrome::ToggleDevToolsWindow(browser(), DEVTOOLS_TOGGLE_ACTION_INSPECT);
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  RenderViewHost* devtools = FindFirstDevToolsHost();
  DCHECK(devtools);

  // DevTools start in a separate process.
  DevToolsWindow::ToggleDevToolsWindow(
      devtools, DEVTOOLS_TOGGLE_ACTION_INSPECT);
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  EXPECT_EQ(host_count, RenderProcessHostCount());
}
