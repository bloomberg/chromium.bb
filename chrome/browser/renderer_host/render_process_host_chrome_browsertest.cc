// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_process_host_browsertest.h"

#include "base/command_line.h"
#include "chrome/browser/debugger/devtools_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"

namespace {

RenderViewHost* FindFirstDevToolsHost() {
  content::RenderProcessHost::iterator hosts =
      content::RenderProcessHost::AllHostsIterator();
  for (; !hosts.IsAtEnd(); hosts.Advance()) {
    content::RenderProcessHost* render_process_host = hosts.GetCurrentValue();
    DCHECK(render_process_host);
    if (!render_process_host->HasConnection())
      continue;
    content::RenderProcessHost::listeners_iterator iter(
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

}  // namespace

// Ensure that DevTools opened to debug DevTools is launched in a separate
// process when --process-per-tab is set. See crbug.com/69873.
IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, DevToolsOnSelfInOwnProcessPPT) {
  CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  parsed_command_line.AppendSwitch(switches::kProcessPerTab);

  int tab_count = 1;
  int host_count = 1;

#if defined(USE_VIRTUAL_KEYBOARD)
  ++host_count;  // For the virtual keyboard.
#endif

  GURL page1("data:text/html,hello world1");
  browser()->ShowSingletonTab(page1);
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
  DevToolsWindow::ToggleDevToolsWindow(
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

#if defined(USE_VIRTUAL_KEYBOARD)
  ++host_count;  // For the virtual keyboard.
#endif

  GURL page1("data:text/html,hello world1");
  browser()->ShowSingletonTab(page1);
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
  DevToolsWindow::ToggleDevToolsWindow(
      devtools, DEVTOOLS_TOGGLE_ACTION_INSPECT);
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  EXPECT_EQ(host_count, RenderProcessHostCount());
}
