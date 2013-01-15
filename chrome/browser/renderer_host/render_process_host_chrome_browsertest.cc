// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
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

class ChromeRenderProcessHostTest : public InProcessBrowserTest {
 public:
  ChromeRenderProcessHostTest() {}

  // Show a tab, activating the current one if there is one, and wait for
  // the renderer process to be created or foregrounded, returning the process
  // handle.
  base::ProcessHandle ShowSingletonTab(const GURL& page) {
    chrome::ShowSingletonTab(browser(), page);
    WebContents* wc = browser()->tab_strip_model()->GetActiveWebContents();
    CHECK(wc->GetURL() == page);

    // Ensure that the backgrounding / foregrounding gets a chance to run.
    content::BrowserThread::PostTaskAndReply(
        content::BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
        base::Bind(&base::DoNothing), MessageLoop::QuitClosure());
    MessageLoop::current()->Run();

    return wc->GetRenderProcessHost()->GetHandle();
  }

  // When we hit the max number of renderers, verify that the way we do process
  // sharing behaves correctly.  In particular, this test is verifying that even
  // when we hit the max process limit, that renderers of each type will wind up
  // in a process of that type, even if that means creating a new process.
  void TestProcessOverflow() {
    int tab_count = 1;
    int host_count = 1;
    WebContents* tab1 = NULL;
    WebContents* tab2 = NULL;
    content::RenderProcessHost* rph1 = NULL;
    content::RenderProcessHost* rph2 = NULL;
    content::RenderProcessHost* rph3 = NULL;

    // Change the first tab to be the new tab page (TYPE_WEBUI).
    GURL newtab(chrome::kChromeUINewTabURL);
    ui_test_utils::NavigateToURL(browser(), newtab);
    EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
    tab1 = browser()->tab_strip_model()->GetWebContentsAt(tab_count - 1);
    rph1 = tab1->GetRenderProcessHost();
    EXPECT_EQ(tab1->GetURL(), newtab);
    EXPECT_EQ(host_count, RenderProcessHostCount());

    // Create a new TYPE_TABBED tab.  It should be in its own process.
    GURL page1("data:text/html,hello world1");

    ui_test_utils::WindowedTabAddedNotificationObserver observer1(
        content::NotificationService::AllSources());
    chrome::ShowSingletonTab(browser(), page1);
    observer1.Wait();

    tab_count++;
    host_count++;
    EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
    tab1 = browser()->tab_strip_model()->GetWebContentsAt(tab_count - 1);
    rph2 = tab1->GetRenderProcessHost();
    EXPECT_EQ(tab1->GetURL(), page1);
    EXPECT_EQ(host_count, RenderProcessHostCount());
    EXPECT_NE(rph1, rph2);

    // Create another TYPE_TABBED tab.  It should share the previous process.
    GURL page2("data:text/html,hello world2");
    ui_test_utils::WindowedTabAddedNotificationObserver observer2(
        content::NotificationService::AllSources());
    chrome::ShowSingletonTab(browser(), page2);
    observer2.Wait();
    tab_count++;
    EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
    tab2 = browser()->tab_strip_model()->GetWebContentsAt(tab_count - 1);
    EXPECT_EQ(tab2->GetURL(), page2);
    EXPECT_EQ(host_count, RenderProcessHostCount());
    EXPECT_EQ(tab2->GetRenderProcessHost(), rph2);

    // Create another TYPE_WEBUI tab.  It should share the process with newtab.
    // Note: intentionally create this tab after the TYPE_TABBED tabs to
    // exercise bug 43448 where extension and WebUI tabs could get combined into
    // normal renderers.
    GURL history(chrome::kChromeUIHistoryURL);
    ui_test_utils::WindowedTabAddedNotificationObserver observer3(
        content::NotificationService::AllSources());
    chrome::ShowSingletonTab(browser(), history);
    observer3.Wait();
    tab_count++;
    EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
    tab2 = browser()->tab_strip_model()->GetWebContentsAt(tab_count - 1);
    EXPECT_EQ(tab2->GetURL(), GURL(history));
    EXPECT_EQ(host_count, RenderProcessHostCount());
    EXPECT_EQ(tab2->GetRenderProcessHost(), rph1);

    // Create a TYPE_EXTENSION tab.  It should be in its own process.
    // (the bookmark manager is implemented as an extension)
    GURL bookmarks(chrome::kChromeUIBookmarksURL);
    ui_test_utils::WindowedTabAddedNotificationObserver observer4(
        content::NotificationService::AllSources());
    chrome::ShowSingletonTab(browser(), bookmarks);
    observer4.Wait();
    tab_count++;
    host_count++;
    EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
    tab1 = browser()->tab_strip_model()->GetWebContentsAt(tab_count - 1);
    rph3 = tab1->GetRenderProcessHost();
    EXPECT_EQ(tab1->GetURL(), bookmarks);
    EXPECT_EQ(host_count, RenderProcessHostCount());
    EXPECT_NE(rph1, rph3);
    EXPECT_NE(rph2, rph3);
  }
};


class ChromeRenderProcessHostTestWithCommandLine
    : public ChromeRenderProcessHostTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(switches::kRendererProcessLimit, "1");
  }
};

IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostTest, ProcessPerTab) {
  // Set max renderers to 1 to force running out of processes.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  parsed_command_line.AppendSwitch(switches::kProcessPerTab);

  int tab_count = 1;
  int host_count = 1;

  // Change the first tab to be the new tab page (TYPE_WEBUI).
  GURL newtab(chrome::kChromeUINewTabURL);
  ui_test_utils::NavigateToURL(browser(), newtab);
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Create a new TYPE_TABBED tab.  It should be in its own process.
  GURL page1("data:text/html,hello world1");
  ui_test_utils::WindowedTabAddedNotificationObserver observer1(
      content::NotificationService::AllSources());
  chrome::ShowSingletonTab(browser(), page1);
  observer1.Wait();
  tab_count++;
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Create another TYPE_TABBED tab.  It should share the previous process.
  GURL page2("data:text/html,hello world2");
  ui_test_utils::WindowedTabAddedNotificationObserver observer2(
      content::NotificationService::AllSources());
  chrome::ShowSingletonTab(browser(), page2);
  observer2.Wait();
  tab_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Create another new tab.  It should share the process with the other WebUI.
  ui_test_utils::WindowedTabAddedNotificationObserver observer3(
      content::NotificationService::AllSources());
  chrome::NewTab(browser());
  observer3.Wait();
  tab_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Create another new tab.  It should share the process with the other WebUI.
  ui_test_utils::WindowedTabAddedNotificationObserver observer4(
      content::NotificationService::AllSources());
  chrome::NewTab(browser());
  observer4.Wait();
  tab_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());
}

// We don't change process priorities on Mac or Posix because the user lacks the
// permission to raise a process' priority even after lowering it.
#if defined(OS_WIN) || defined(OS_LINUX)
IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostTest, Backgrounding) {
  if (!base::Process::CanBackgroundProcesses()) {
    LOG(ERROR) << "Can't background processes";
    return;
  }
  CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  parsed_command_line.AppendSwitch(switches::kProcessPerTab);

  // Change the first tab to be the new tab page (TYPE_WEBUI).
  GURL newtab(chrome::kChromeUINewTabURL);
  ui_test_utils::NavigateToURL(browser(), newtab);

  // Create a new tab. It should be foreground.
  GURL page1("data:text/html,hello world1");
  base::ProcessHandle pid1 = ShowSingletonTab(page1);
  EXPECT_FALSE(base::Process(pid1).IsProcessBackgrounded());

  // Create another tab. It should be foreground, and the first tab should
  // now be background.
  GURL page2("data:text/html,hello world2");
  base::ProcessHandle pid2 = ShowSingletonTab(page2);
  EXPECT_NE(pid1, pid2);
  EXPECT_TRUE(base::Process(pid1).IsProcessBackgrounded());
  EXPECT_FALSE(base::Process(pid2).IsProcessBackgrounded());

  // Navigate back to first page. It should be foreground again, and the second
  // tab should be background.
  EXPECT_EQ(pid1, ShowSingletonTab(page1));
  EXPECT_FALSE(base::Process(pid1).IsProcessBackgrounded());
  EXPECT_TRUE(base::Process(pid2).IsProcessBackgrounded());
}
#endif

IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostTest, ProcessOverflow) {
  // Set max renderers to 1 to force running out of processes.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);
  TestProcessOverflow();
}

// Variation of the ProcessOverflow test, which is driven through command line
// parameter instead of direct function call into the class.
IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostTestWithCommandLine,
                       ProcessOverflow) {
  TestProcessOverflow();
}

// Ensure that DevTools opened to debug DevTools is launched in a separate
// process when --process-per-tab is set. See crbug.com/69873.
IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostTest,
                       DevToolsOnSelfInOwnProcessPPT) {
  CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  parsed_command_line.AppendSwitch(switches::kProcessPerTab);

  int tab_count = 1;
  int host_count = 1;

  GURL page1("data:text/html,hello world1");
  ui_test_utils::WindowedTabAddedNotificationObserver observer1(
      content::NotificationService::AllSources());
  chrome::ShowSingletonTab(browser(), page1);
  observer1.Wait();
  tab_count++;
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // DevTools start in docked mode (no new tab), in a separate process.
  chrome::ToggleDevToolsWindow(browser(), DEVTOOLS_TOGGLE_ACTION_INSPECT);
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  RenderViewHost* devtools = FindFirstDevToolsHost();
  DCHECK(devtools);

  // DevTools start in a separate process.
  DevToolsWindow::ToggleDevToolsWindow(
      devtools, true, DEVTOOLS_TOGGLE_ACTION_INSPECT);
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());
}

// Ensure that DevTools opened to debug DevTools is launched in a separate
// process. See crbug.com/69873.
IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostTest,
                       DevToolsOnSelfInOwnProcess) {
  int tab_count = 1;
  int host_count = 1;

  GURL page1("data:text/html,hello world1");
  ui_test_utils::WindowedTabAddedNotificationObserver observer1(
      content::NotificationService::AllSources());
  chrome::ShowSingletonTab(browser(), page1);
  observer1.Wait();
  tab_count++;
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // DevTools start in docked mode (no new tab), in a separate process.
  chrome::ToggleDevToolsWindow(browser(), DEVTOOLS_TOGGLE_ACTION_INSPECT);
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  RenderViewHost* devtools = FindFirstDevToolsHost();
  DCHECK(devtools);

  // DevTools start in a separate process.
  DevToolsWindow::ToggleDevToolsWindow(
      devtools, true, DEVTOOLS_TOGGLE_ACTION_INSPECT);
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());
}
