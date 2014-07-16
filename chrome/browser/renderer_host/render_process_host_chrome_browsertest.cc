// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"

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
  scoped_ptr<content::RenderWidgetHostIterator> widgets(
      RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
    if (!widget->GetProcess()->HasConnection())
      continue;
    if (!widget->IsRenderView())
      continue;
    RenderViewHost* host = RenderViewHost::From(widget);
    WebContents* contents = WebContents::FromRenderViewHost(host);
    GURL url = contents->GetURL();
    if (url.SchemeIs(content::kChromeDevToolsScheme))
      return host;
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

    WaitForLauncherThread();
    WaitForMessageProcessing(wc);
    return wc->GetRenderProcessHost()->GetHandle();
  }

  // Loads the given url in a new background tab and returns the handle of its
  // renderer.
  base::ProcessHandle OpenBackgroundTab(const GURL& page) {
    ui_test_utils::NavigateToURLWithDisposition(browser(), page,
        NEW_BACKGROUND_TAB, ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

    TabStripModel* tab_strip = browser()->tab_strip_model();
    WebContents* wc = tab_strip->GetWebContentsAt(
        tab_strip->active_index() + 1);
    CHECK(wc->GetVisibleURL() == page);

    WaitForLauncherThread();
    WaitForMessageProcessing(wc);
    return wc->GetRenderProcessHost()->GetHandle();
  }

  // Ensures that the backgrounding / foregrounding gets a chance to run.
  void WaitForLauncherThread() {
    content::BrowserThread::PostTaskAndReply(
        content::BrowserThread::PROCESS_LAUNCHER, FROM_HERE,
        base::Bind(&base::DoNothing), base::MessageLoop::QuitClosure());
    base::MessageLoop::current()->Run();
  }

  // Implicitly waits for the renderer process associated with the specified
  // WebContents to process outstanding IPC messages by running some JavaScript
  // and waiting for the result.
  void WaitForMessageProcessing(WebContents* wc) {
    bool result = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        wc, "window.domAutomationController.send(true);", &result));
    ASSERT_TRUE(result);
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

    // Change the first tab to be the omnibox page (TYPE_WEBUI).
    GURL omnibox(chrome::kChromeUIOmniboxURL);
    ui_test_utils::NavigateToURL(browser(), omnibox);
    EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
    tab1 = browser()->tab_strip_model()->GetWebContentsAt(tab_count - 1);
    rph1 = tab1->GetRenderProcessHost();
    EXPECT_EQ(omnibox, tab1->GetURL());
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

    // Create another TYPE_WEBUI tab.  It should share the process with omnibox.
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
  GURL omnibox(chrome::kChromeUIOmniboxURL);
  ui_test_utils::NavigateToURL(browser(), omnibox);
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

  // Create another omnibox tab.  It should share the process with the other
  // WebUI.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), omnibox, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  tab_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Create another omnibox tab.  It should share the process with the other
  // WebUI.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), omnibox, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  tab_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());
}

// We don't change process priorities on Mac or Posix because the user lacks the
// permission to raise a process' priority even after lowering it.
#if defined(OS_WIN) || defined(OS_LINUX)
#if defined(OS_WIN)
// Flaky test: crbug.com/394368
#define MAYBE_Backgrounding DISABLED_Backgrounding
#else
#define MAYBE_Backgrounding Backgrounding
#endif
IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostTest, MAYBE_Backgrounding) {
  if (!base::Process::CanBackgroundProcesses()) {
    LOG(ERROR) << "Can't background processes";
    return;
  }
  CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  parsed_command_line.AppendSwitch(switches::kProcessPerTab);

  // Change the first tab to be the omnibox page (TYPE_WEBUI).
  GURL omnibox(chrome::kChromeUIOmniboxURL);
  ui_test_utils::NavigateToURL(browser(), omnibox);

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

  // Load another tab in background. The renderer of the new tab should be
  // backgrounded, while visibility of the other renderers should not change.
  GURL page3("data:text/html,hello world3");
  base::ProcessHandle pid3 = OpenBackgroundTab(page3);
  EXPECT_NE(pid3, pid1);
  EXPECT_NE(pid3, pid2);
  EXPECT_TRUE(base::Process(pid1).IsProcessBackgrounded());
  EXPECT_FALSE(base::Process(pid2).IsProcessBackgrounded());
  EXPECT_TRUE(base::Process(pid3).IsProcessBackgrounded());

  // Navigate back to the first page. Its renderer should be in foreground
  // again while the other renderers should be backgrounded.
  EXPECT_EQ(pid1, ShowSingletonTab(page1));
  EXPECT_FALSE(base::Process(pid1).IsProcessBackgrounded());
  EXPECT_TRUE(base::Process(pid2).IsProcessBackgrounded());
  EXPECT_TRUE(base::Process(pid3).IsProcessBackgrounded());
}
#endif

// TODO(nasko): crbug.com/173137
#if defined(OS_WIN)
#define MAYBE_ProcessOverflow DISABLED_ProcessOverflow
#else
#define MAYBE_ProcessOverflow ProcessOverflow
#endif

IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostTest, MAYBE_ProcessOverflow) {
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
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

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
  chrome::ToggleDevToolsWindow(browser(), DevToolsToggleAction::Inspect());
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  RenderViewHost* devtools = FindFirstDevToolsHost();
  DCHECK(devtools);

  // DevTools start in a separate process.
  DevToolsWindow::OpenDevToolsWindow(devtools, DevToolsToggleAction::Inspect());
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // close docked devtools
  content::WindowedNotificationObserver close_observer(
      content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::Source<WebContents>(WebContents::FromRenderViewHost(devtools)));

  chrome::ToggleDevToolsWindow(browser(), DevToolsToggleAction::Toggle());
  close_observer.Wait();
}

// Ensure that DevTools opened to debug DevTools is launched in a separate
// process. See crbug.com/69873.
IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostTest,
                       DevToolsOnSelfInOwnProcess) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

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
  chrome::ToggleDevToolsWindow(browser(), DevToolsToggleAction::Inspect());
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  RenderViewHost* devtools = FindFirstDevToolsHost();
  DCHECK(devtools);

  // DevTools start in a separate process.
  DevToolsWindow::OpenDevToolsWindow(devtools, DevToolsToggleAction::Inspect());
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // close docked devtools
  content::WindowedNotificationObserver close_observer(
      content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::Source<content::WebContents>(
          WebContents::FromRenderViewHost(devtools)));
  chrome::ToggleDevToolsWindow(browser(), DevToolsToggleAction::Toggle());
  close_observer.Wait();
}

// This class's goal is to close the browser window when a renderer process has
// crashed. It does so by monitoring WebContents for RenderProcessGone event and
// closing the passed in TabStripModel. This is used in the following test case.
class WindowDestroyer : public content::WebContentsObserver {
 public:
  WindowDestroyer(content::WebContents* web_contents, TabStripModel* model)
      : content::WebContentsObserver(web_contents),
        tab_strip_model_(model) {
  }

  virtual void RenderProcessGone(base::TerminationStatus status) OVERRIDE {
    // Wait for the window to be destroyed, which will ensure all other
    // RenderViewHost objects are deleted before we return and proceed with
    // the next iteration of notifications.
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        content::NotificationService::AllSources());
    tab_strip_model_->CloseAllTabs();
    observer.Wait();
  }

 private:
  TabStripModel* tab_strip_model_;

  DISALLOW_COPY_AND_ASSIGN(WindowDestroyer);
};

// Test to ensure that while iterating through all listeners in
// RenderProcessHost and invalidating them, we remove them properly and don't
// access already freed objects. See http://crbug.com/255524.
IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostTest,
                       CloseAllTabsDuringProcessDied) {
  GURL url(chrome::kChromeUIOmniboxURL);

  ui_test_utils::NavigateToURL(browser(), url);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  WebContents* wc1 = browser()->tab_strip_model()->GetWebContentsAt(0);
  WebContents* wc2 = browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_EQ(wc1->GetRenderProcessHost(), wc2->GetRenderProcessHost());

  // Create an object that will close the window on a process crash.
  WindowDestroyer destroyer(wc1, browser()->tab_strip_model());

  // Use NOTIFICATION_BROWSER_CLOSED instead of NOTIFICATION_WINDOW_CLOSED,
  // since the latter is not implemented on OSX and the test will timeout,
  // causing it to fail.
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::NotificationService::AllSources());

  // Kill the renderer process, simulating a crash. This should the ProcessDied
  // method to be called. Alternatively, RenderProcessHost::OnChannelError can
  // be called to directly force a call to ProcessDied.
  base::KillProcess(wc1->GetRenderProcessHost()->GetHandle(), -1, true);

  observer.Wait();
}
