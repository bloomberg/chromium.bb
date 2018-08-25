// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/child_process_launcher_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "media/base/media_switches.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if defined(OS_MACOSX)
#include "content/public/browser/browser_child_process_host.h"
#endif  // defined(OS_MACOSX)

using content::RenderViewHost;
using content::RenderWidgetHost;
using content::WebContents;

namespace {

int RenderProcessHostCount() {
  return content::RenderProcessHost::GetCurrentRenderProcessCountForTesting();
}

WebContents* FindFirstDevToolsContents() {
  std::unique_ptr<content::RenderWidgetHostIterator> widgets(
      RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
    if (!widget->GetProcess()->IsInitializedAndNotDead())
      continue;
    RenderViewHost* view_host = RenderViewHost::From(widget);
    if (!view_host)
      continue;
    WebContents* contents = WebContents::FromRenderViewHost(view_host);
    GURL url = contents->GetURL();
    if (url.SchemeIs(content::kChromeDevToolsScheme))
      return contents;
  }
  return NULL;
}

// TODO(rvargas) crbug.com/417532: Remove this code.
base::Process ProcessFromHandle(base::ProcessHandle handle) {
#if defined(OS_WIN)
  if (handle == GetCurrentProcess())
    return base::Process::Current();

  base::ProcessHandle out_handle;
  if (!::DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(),
                         &out_handle, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
    return base::Process();
  }
  handle = out_handle;
#endif  // defined(OS_WIN)
  return base::Process(handle);
}

}  // namespace

class ChromeRenderProcessHostTest : public extensions::ExtensionBrowserTest {
 public:
  ChromeRenderProcessHostTest() {}

  // Show a tab, activating the current one if there is one, and wait for
  // the renderer process to be created or foregrounded, returning the process
  // handle.
  base::Process ShowSingletonTab(const GURL& page) {
    ::ShowSingletonTab(browser(), page);
    WebContents* wc = browser()->tab_strip_model()->GetActiveWebContents();
    CHECK(wc->GetURL() == page);

    WaitForLauncherThread();
    WaitForMessageProcessing(wc);
    return ProcessFromHandle(
        wc->GetMainFrame()->GetProcess()->GetProcess().Handle());
  }

  // Loads the given url in a new background tab and returns the handle of its
  // renderer.
  base::Process OpenBackgroundTab(const GURL& page) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), page, WindowOpenDisposition::NEW_BACKGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

    TabStripModel* tab_strip = browser()->tab_strip_model();
    WebContents* wc =
        tab_strip->GetWebContentsAt(tab_strip->active_index() + 1);
    CHECK(wc->GetVisibleURL() == page);

    WaitForLauncherThread();
    WaitForMessageProcessing(wc);
    return ProcessFromHandle(
        wc->GetMainFrame()->GetProcess()->GetProcess().Handle());
  }

  // Ensures that the backgrounding / foregrounding gets a chance to run.
  void WaitForLauncherThread() {
    base::RunLoop run_loop;
    content::GetProcessLauncherTaskRunner()->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), run_loop.QuitWhenIdleClosure());
    run_loop.Run();
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

    const extensions::Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII("options_page"));

    // Change the first tab to be the omnibox page (WebUI).
    GURL omnibox(chrome::kChromeUIOmniboxURL);
    ui_test_utils::NavigateToURL(browser(), omnibox);
    EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
    tab1 = browser()->tab_strip_model()->GetWebContentsAt(tab_count - 1);
    rph1 = tab1->GetMainFrame()->GetProcess();
    EXPECT_EQ(omnibox, tab1->GetURL());
    EXPECT_EQ(host_count, RenderProcessHostCount());

    // Create a new normal tab with a data URL. It should be in its own process.
    GURL page1("data:text/html,hello world1");

    ui_test_utils::WindowedTabAddedNotificationObserver observer1(
        content::NotificationService::AllSources());
    ::ShowSingletonTab(browser(), page1);
    observer1.Wait();

    tab_count++;
    host_count++;
    EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
    tab1 = browser()->tab_strip_model()->GetWebContentsAt(tab_count - 1);
    rph2 = tab1->GetMainFrame()->GetProcess();
    EXPECT_EQ(tab1->GetURL(), page1);
    EXPECT_EQ(host_count, RenderProcessHostCount());
    EXPECT_NE(rph1, rph2);

    // Create another data URL tab.  With Site Isolation, this will require its
    // own process, but without Site Isolation, it can share the previous
    // process.
    GURL page2("data:text/html,hello world2");
    ui_test_utils::WindowedTabAddedNotificationObserver observer2(
        content::NotificationService::AllSources());
    ::ShowSingletonTab(browser(), page2);
    observer2.Wait();
    tab_count++;
    if (content::AreAllSitesIsolatedForTesting())
      host_count++;
    EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
    tab2 = browser()->tab_strip_model()->GetWebContentsAt(tab_count - 1);
    EXPECT_EQ(tab2->GetURL(), page2);
    EXPECT_EQ(host_count, RenderProcessHostCount());
    if (content::AreAllSitesIsolatedForTesting())
      EXPECT_NE(tab2->GetMainFrame()->GetProcess(), rph2);
    else
      EXPECT_EQ(tab2->GetMainFrame()->GetProcess(), rph2);

    // Create another WebUI tab.  It should share the process with omnibox.
    // Note: intentionally create this tab after the normal tabs to exercise bug
    // 43448 where extension and WebUI tabs could get combined into normal
    // renderers.
    GURL history(chrome::kChromeUIHistoryURL);
    ui_test_utils::WindowedTabAddedNotificationObserver observer3(
        content::NotificationService::AllSources());
    ::ShowSingletonTab(browser(), history);
    observer3.Wait();
    tab_count++;
    EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
    tab2 = browser()->tab_strip_model()->GetWebContentsAt(tab_count - 1);
    EXPECT_EQ(tab2->GetURL(), GURL(history));
    EXPECT_EQ(host_count, RenderProcessHostCount());
    EXPECT_EQ(tab2->GetMainFrame()->GetProcess(), rph1);

    // Create an extension tab.  It should be in its own process.
    GURL extension_url("chrome-extension://" + extension->id());
    ui_test_utils::WindowedTabAddedNotificationObserver observer4(
        content::NotificationService::AllSources());
    ::ShowSingletonTab(browser(), extension_url);

    observer4.Wait();
    tab_count++;
    host_count++;
    EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
    tab1 = browser()->tab_strip_model()->GetWebContentsAt(tab_count - 1);
    rph3 = tab1->GetMainFrame()->GetProcess();
    EXPECT_EQ(tab1->GetURL(), extension_url);
    EXPECT_EQ(host_count, RenderProcessHostCount());
    EXPECT_NE(rph1, rph3);
    EXPECT_NE(rph2, rph3);
  }
};

class ChromeRenderProcessHostTestWithCommandLine
    : public ChromeRenderProcessHostTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeRenderProcessHostTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kRendererProcessLimit, "1");
  }
};

// Disable on Windows and Mac due to ongoing flakiness. (crbug.com/442785)
#if defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_ProcessPerTab DISABLED_ProcessPerTab
#else
#define MAYBE_ProcessPerTab ProcessPerTab
#endif

IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostTest, MAYBE_ProcessPerTab) {
  // Set max renderers to 1 to force running out of processes.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  base::CommandLine& parsed_command_line =
      *base::CommandLine::ForCurrentProcess();
  parsed_command_line.AppendSwitch(switches::kProcessPerTab);

  int tab_count = 1;
  int host_count = 1;

  // Change the first tab to be a WebUI page.
  GURL omnibox(chrome::kChromeUIOmniboxURL);
  ui_test_utils::NavigateToURL(browser(), omnibox);
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Create a new normal tab with a data URL.  It should be in its own process.
  GURL page1("data:text/html,hello world1");
  ui_test_utils::WindowedTabAddedNotificationObserver observer1(
      content::NotificationService::AllSources());
  ::ShowSingletonTab(browser(), page1);
  observer1.Wait();
  tab_count++;
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Create another data URL tab.  With Site Isolation, this will require its
  // own process, but without Site Isolation, it can share the previous process.
  GURL page2("data:text/html,hello world2");
  ui_test_utils::WindowedTabAddedNotificationObserver observer2(
      content::NotificationService::AllSources());
  ::ShowSingletonTab(browser(), page2);
  observer2.Wait();
  tab_count++;
  if (content::AreAllSitesIsolatedForTesting())
    host_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Create another omnibox tab.  It should share the process with the other
  // WebUI.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), omnibox, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  tab_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Create another omnibox tab.  It should share the process with the other
  // WebUI.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), omnibox, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  tab_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());
}

class ChromeRenderProcessHostBackgroundingTest
    : public ChromeRenderProcessHostTest,
      public testing::WithParamInterface<std::string> {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeRenderProcessHostTest::SetUpCommandLine(command_line);

    command_line->AppendSwitch(switches::kProcessPerTab);

    command_line->AppendSwitchASCII(
        switches::kForceFieldTrials,
        "BoostRendererPriorityForPendingViews/" + GetParam());
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

IN_PROC_BROWSER_TEST_P(ChromeRenderProcessHostBackgroundingTest, MultipleTabs) {
  // This test is invalid on platforms that can't background.
  if (!base::Process::CanBackgroundProcesses())
    return;

#if defined(OS_MACOSX)
  base::PortProvider* port_provider =
      content::BrowserChildProcessHost::GetPortProvider();
#endif  //  defined(OS_MACOSX)

  // Change the first tab to be the omnibox page (TYPE_WEBUI).
  GURL omnibox(chrome::kChromeUIOmniboxURL);
  ui_test_utils::NavigateToURL(browser(), omnibox);

  // Create a new tab. It should be foreground.
  GURL page1("data:text/html,hello world1");
  base::Process process1 = ShowSingletonTab(page1);
  ASSERT_TRUE(process1.IsValid());
#if defined(OS_MACOSX)
  EXPECT_FALSE(process1.IsProcessBackgrounded(port_provider));
#else
  EXPECT_FALSE(process1.IsProcessBackgrounded());
#endif

  // Create another tab. It should be foreground, and the first tab should
  // now be background.
  GURL page2("data:text/html,hello world2");
  base::Process process2 = ShowSingletonTab(page2);
  ASSERT_TRUE(process2.IsValid());
  EXPECT_NE(process1.Pid(), process2.Pid());
#if defined(OS_MACOSX)
  EXPECT_TRUE(process1.IsProcessBackgrounded(port_provider));
  EXPECT_FALSE(process2.IsProcessBackgrounded(port_provider));
#else
  EXPECT_TRUE(process1.IsProcessBackgrounded());
  EXPECT_FALSE(process2.IsProcessBackgrounded());
#endif  // defined(OS_MACOSX)

  // Load another tab in background. The renderer of the new tab should be
  // backgrounded, while visibility of the other renderers should not change.
  GURL page3("data:text/html,hello world3");
  base::Process process3 = OpenBackgroundTab(page3);
  ASSERT_TRUE(process3.IsValid());
  EXPECT_NE(process3.Pid(), process1.Pid());
  EXPECT_NE(process3.Pid(), process2.Pid());
#if defined(OS_MACOSX)
  EXPECT_TRUE(process1.IsProcessBackgrounded(port_provider));
  EXPECT_FALSE(process2.IsProcessBackgrounded(port_provider));
  // TODO(gab): The new background tab should be backgrounded but it currently
  // only is under an experiment.
  EXPECT_EQ(process3.IsProcessBackgrounded(port_provider),
            GetParam() == "Enabled");

  // Navigate back to the first page. Its renderer should be in foreground
  // again while the other renderers should be backgrounded.

  EXPECT_EQ(process1.Pid(), ShowSingletonTab(page1).Pid());
  EXPECT_FALSE(process1.IsProcessBackgrounded(port_provider));
  EXPECT_TRUE(process2.IsProcessBackgrounded(port_provider));
  // TODO(gab): Same as above.
  EXPECT_EQ(process3.IsProcessBackgrounded(port_provider),
            GetParam() == "Enabled");

  // TODO(gab): Remove this when https://crbug.com/560446 is fixed, but for now
  // confirm that the correct state is at least achieved, regardless of the
  // experiment state, when tab #3 is explicitly foregrounded and
  // re-backgrounded.
  EXPECT_EQ(process3.Pid(), ShowSingletonTab(page3).Pid());
  EXPECT_EQ(process1.Pid(), ShowSingletonTab(page1).Pid());
  EXPECT_FALSE(process1.IsProcessBackgrounded(port_provider));
  EXPECT_TRUE(process2.IsProcessBackgrounded(port_provider));
  EXPECT_TRUE(process3.IsProcessBackgrounded(port_provider));
#else
  EXPECT_TRUE(process1.IsProcessBackgrounded());
  EXPECT_FALSE(process2.IsProcessBackgrounded());
  // TODO(gab): The new background tab should be backgrounded but it currently
  // only is under an experiment.
  EXPECT_EQ(process3.IsProcessBackgrounded(), GetParam() == "Enabled");

  // Navigate back to the first page. Its renderer should be in foreground
  // again while the other renderers should be backgrounded.

  EXPECT_EQ(process1.Pid(), ShowSingletonTab(page1).Pid());
  EXPECT_FALSE(process1.IsProcessBackgrounded());
  EXPECT_TRUE(process2.IsProcessBackgrounded());
  // TODO(gab): Same as above.
  EXPECT_EQ(process3.IsProcessBackgrounded(), GetParam() == "Enabled");

  // TODO(gab): Remove this when https://crbug.com/560446 is fixed, but for now
  // confirm that the correct state is at least achieved, regardless of the
  // experiment state, when tab #3 is explicitly foregrounded and
  // re-backgrounded.
  EXPECT_EQ(process3.Pid(), ShowSingletonTab(page3).Pid());
  EXPECT_EQ(process1.Pid(), ShowSingletonTab(page1).Pid());
  EXPECT_FALSE(process1.IsProcessBackgrounded());
  EXPECT_TRUE(process2.IsProcessBackgrounded());
  EXPECT_TRUE(process3.IsProcessBackgrounded());
#endif
}

namespace {

// A helper to post a recurring check that a renderer is foregrounded. The
// recurring check uses WeakPtr semantic and will die when this class goes out
// of scope.
class AssertForegroundHelper {
 public:
  AssertForegroundHelper() : weak_ptr_factory_(this) {}

#if defined(OS_MACOSX)
  // Asserts that |renderer_process| isn't backgrounded and reposts self to
  // check again shortly. |renderer_process| must outlive this
  // AssertForegroundHelper instance.
  void AssertForegroundAndRepost(const base::Process& renderer_process,
                                 base::PortProvider* port_provider) {
    ASSERT_FALSE(renderer_process.IsProcessBackgrounded(port_provider));
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AssertForegroundHelper::AssertForegroundAndRepost,
                       weak_ptr_factory_.GetWeakPtr(),
                       base::ConstRef(renderer_process), port_provider),
        base::TimeDelta::FromMilliseconds(1));
  }
#else   // defined(OS_MACOSX)
  // Same as above without the Mac specific base::PortProvider.
  void AssertForegroundAndRepost(const base::Process& renderer_process) {
    ASSERT_FALSE(renderer_process.IsProcessBackgrounded());
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AssertForegroundHelper::AssertForegroundAndRepost,
                       weak_ptr_factory_.GetWeakPtr(),
                       base::ConstRef(renderer_process)),
        base::TimeDelta::FromMilliseconds(1));
  }
#endif  // defined(OS_MACOSX)

 private:
  base::WeakPtrFactory<AssertForegroundHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AssertForegroundHelper);
};

}  // namespace

// This is a regression test for https://crbug.com/560446 (ensures a foreground
// navigation isn't backgrounded prior to its navigation committing a "visible"
// widget). TODO(gab): This test should move to
// content/browser/renderer_host/render_process_host_browsertest.cc after the
// experiment (there's nothing chrome specific about it really).
IN_PROC_BROWSER_TEST_P(ChromeRenderProcessHostBackgroundingTest,
                       ForegroundNavigationIsNeverBackgrounded) {
#if defined(OS_MACOSX)
  base::PortProvider* port_provider =
      content::BrowserChildProcessHost::GetPortProvider();
#endif  //  defined(OS_MACOSX)

  GURL url(embedded_test_server()->GetURL("/title1.html"));
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::TestNavigationManager navigation_manager(web_contents, url);

  content::NavigationController::LoadURLParams load_params(url);
  web_contents->GetController().LoadURLWithParams(load_params);

  const base::Process& renderer_process =
      web_contents->GetMainFrame()->GetProcess()->GetProcess();
  ASSERT_TRUE(renderer_process.IsValid());

  // Kick off an infinite check against self that the tab under navigation is
  // never backgrounded (the upcoming WaitforResponse() will wait inside a
  // RunLoop() and hence perform this check regularly throughout the
  // navigation).
  AssertForegroundHelper assert_foreground_helper;
#if defined(OS_MACOSX)
  assert_foreground_helper.AssertForegroundAndRepost(renderer_process,
                                                     port_provider);
#else
  assert_foreground_helper.AssertForegroundAndRepost(renderer_process);
#endif

  // Wait until the response (while performing the above check).
  ASSERT_TRUE(navigation_manager.WaitForResponse());

  // Then perform the check once more after making sure the queues have been
  // flushed.
  WaitForLauncherThread();
  WaitForMessageProcessing(web_contents);
  ASSERT_EQ(renderer_process.Handle(),
            web_contents->GetMainFrame()->GetProcess()->GetProcess().Handle());
#if defined(OS_MACOSX)
  ASSERT_FALSE(renderer_process.IsProcessBackgrounded(port_provider));
#else
  ASSERT_FALSE(renderer_process.IsProcessBackgrounded());
#endif

  // And same thing until the navigation is complete.
  navigation_manager.WaitForNavigationFinished();
  WaitForLauncherThread();
  WaitForMessageProcessing(web_contents);
  ASSERT_EQ(renderer_process.Handle(),
            web_contents->GetMainFrame()->GetProcess()->GetProcess().Handle());
#if defined(OS_MACOSX)
  ASSERT_FALSE(renderer_process.IsProcessBackgrounded(port_provider));
#else
  ASSERT_FALSE(renderer_process.IsProcessBackgrounded());
#endif
}

INSTANTIATE_TEST_CASE_P(BoostRendererPriorityForPendingViews,
                        ChromeRenderProcessHostBackgroundingTest,
                        testing::Values("Enabled", "Default"));

// TODO(nasko): crbug.com/173137
// Disable on Windows and Mac due to ongoing flakiness. (crbug.com/442785)
#if defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_ProcessOverflow DISABLED_ProcessOverflow
#else
#define MAYBE_ProcessOverflow ProcessOverflow
#endif

IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostTest, MAYBE_ProcessOverflow) {
  // Set max renderers to 1 to force running out of processes.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);
  TestProcessOverflow();
}

// Disable on Windows and Mac due to ongoing flakiness. (crbug.com/442785)
#if defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_ProcessOverflowCommandLine DISABLED_ProcessOverflowCommandLine
#else
#define MAYBE_ProcessOverflowCommandLine ProcessOverflowCommandLine
#endif

// Variation of the ProcessOverflow test, which is driven through command line
// parameter instead of direct function call into the class.
IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostTestWithCommandLine,
                       MAYBE_ProcessOverflowCommandLine) {
  TestProcessOverflow();
}

// Ensure that DevTools opened to debug DevTools is launched in a separate
// process when --process-per-tab is set. See crbug.com/69873.
IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostTest,
                       DevToolsOnSelfInOwnProcessPPT) {
  base::CommandLine& parsed_command_line =
      *base::CommandLine::ForCurrentProcess();
  parsed_command_line.AppendSwitch(switches::kProcessPerTab);

  int tab_count = 1;
  int host_count = 1;

  GURL page1("data:text/html,hello world1");
  ui_test_utils::WindowedTabAddedNotificationObserver observer1(
      content::NotificationService::AllSources());
  ::ShowSingletonTab(browser(), page1);
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

  WebContents* devtools = FindFirstDevToolsContents();
  DCHECK(devtools);

  // DevTools start in a separate process.
  DevToolsWindow::OpenDevToolsWindow(devtools, DevToolsToggleAction::Inspect());
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // close docked devtools
  content::WindowedNotificationObserver close_observer(
      content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::Source<WebContents>(devtools));

  chrome::ToggleDevToolsWindow(browser(), DevToolsToggleAction::Toggle());
  close_observer.Wait();
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
  ::ShowSingletonTab(browser(), page1);
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

  WebContents* devtools = FindFirstDevToolsContents();
  DCHECK(devtools);

  // DevTools start in a separate process.
  DevToolsWindow::OpenDevToolsWindow(devtools, DevToolsToggleAction::Inspect());
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // close docked devtools
  content::WindowedNotificationObserver close_observer(
      content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::Source<content::WebContents>(devtools));
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
        tab_strip_model_(model),
        browser_closed_observer_(chrome::NOTIFICATION_BROWSER_CLOSED,
                                 content::NotificationService::AllSources()) {}

  // Wait for the browser window to be destroyed.
  void Wait() { browser_closed_observer_.Wait(); }

  void RenderProcessGone(base::TerminationStatus status) override {
    tab_strip_model_->CloseAllTabs();
  }

 private:
  TabStripModel* tab_strip_model_;
  content::WindowedNotificationObserver browser_closed_observer_;

  DISALLOW_COPY_AND_ASSIGN(WindowDestroyer);
};

// Test to ensure that while iterating through all listeners in
// RenderProcessHost and invalidating them, we remove them properly and don't
// access already freed objects. See http://crbug.com/255524.
// Crashes on Win/Linux only.  http://crbug.com/606485.
#if defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_CloseAllTabsDuringProcessDied \
  DISABLED_CloseAllTabsDuringProcessDied
#else
#define MAYBE_CloseAllTabsDuringProcessDied CloseAllTabsDuringProcessDied
#endif
IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostTest,
                       MAYBE_CloseAllTabsDuringProcessDied) {
  GURL url(chrome::kChromeUIOmniboxURL);

  ui_test_utils::NavigateToURL(browser(), url);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  WebContents* wc1 = browser()->tab_strip_model()->GetWebContentsAt(0);
  WebContents* wc2 = browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_EQ(wc1->GetMainFrame()->GetProcess(),
            wc2->GetMainFrame()->GetProcess());

  // Create an object that will close the window on a process crash.
  WindowDestroyer destroyer(wc1, browser()->tab_strip_model());

  // Kill the renderer process, simulating a crash. This should the ProcessDied
  // method to be called. Alternatively, RenderProcessHost::OnChannelError can
  // be called to directly force a call to ProcessDied.
  wc1->GetMainFrame()->GetProcess()->Shutdown(-1);

  destroyer.Wait();
}

// Sets up the browser in order to start the tests with two tabs open: one
// called "no audio" in foreground and another called "audio" in background with
// audio in playing state. Also sets up the variables containing the process
// associated with each tab, the urls of the two pages and the WebContents of
// the "audio" page.
class ChromeRenderProcessHostBackgroundingTestWithAudio
    : public ChromeRenderProcessHostTest {
 public:
  ChromeRenderProcessHostBackgroundingTestWithAudio() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeRenderProcessHostTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kProcessPerTab);

    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);
  }

  void SetUpOnMainThread() override {
    ChromeRenderProcessHostTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    // Set up the server and get the test pages.
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir.AppendASCII("chrome/test/data/"));
    audio_url_ = embedded_test_server()->GetURL("/extensions/loop_audio.html");
    no_audio_url_ = embedded_test_server()->GetURL("/title1.html");

    embedded_test_server()->StartAcceptingConnections();

    // Open a browser, navigate to the audio page and get its WebContents.
    ui_test_utils::NavigateToURL(browser(), audio_url_);
    audio_tab_web_contents_ =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Create a new tab for the no audio page and confirm that the process of
    // each tab is different and that both are valid.
    audio_process_ = ProcessFromHandle(audio_tab_web_contents_->GetMainFrame()
                                           ->GetProcess()
                                           ->GetProcess()
                                           .Handle());
    no_audio_process_ = ShowSingletonTab(no_audio_url_);
    ASSERT_NE(audio_process_.Pid(), no_audio_process_.Pid());
    ASSERT_TRUE(no_audio_process_.IsValid());
    ASSERT_TRUE(audio_process_.IsValid());
#if defined(OS_MACOSX)
    port_provider_ = content::BrowserChildProcessHost::GetPortProvider();
#endif  //  defined(OS_MACOSX)
  }

 protected:
  void WaitUntilBackgrounded(const base::Process& lhs,
                             bool lhs_backgrounded,
                             const base::Process& rhs,
                             bool rhs_backgrounded) {
    while (IsProcessBackgrounded(lhs) != lhs_backgrounded ||
           IsProcessBackgrounded(rhs) != rhs_backgrounded) {
      base::RunLoop().RunUntilIdle();
      base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());
    }
  }

  GURL audio_url_;
  GURL no_audio_url_;

  base::Process audio_process_;
  base::Process no_audio_process_;

  content::WebContents* audio_tab_web_contents_;

 private:
  bool IsProcessBackgrounded(const base::Process& process) {
#if defined(OS_MACOSX)
    return process.IsProcessBackgrounded(port_provider_);
#else
    return process.IsProcessBackgrounded();
#endif
  }

#if defined(OS_MACOSX)
  base::PortProvider* port_provider_;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChromeRenderProcessHostBackgroundingTestWithAudio);
};

// Test to make sure that a process is backgrounded when the audio stops playing
// from the active tab and there is an immediate tab switch.
IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostBackgroundingTestWithAudio,
                       ProcessPriorityAfterStoppedAudio) {
  // This test is invalid on platforms that can't background.
  if (!base::Process::CanBackgroundProcesses())
    return;

  ShowSingletonTab(audio_url_);

  // Wait until the no audio page is backgrounded and the audio page is not
  // backgrounded.
  WaitUntilBackgrounded(no_audio_process_, true, audio_process_, false);
  // Pause the audio and immediately switch to the no audio tab.
  ASSERT_TRUE(content::ExecuteScript(
      audio_tab_web_contents_,
      "document.getElementById('audioPlayer').pause();"));
  ShowSingletonTab(no_audio_url_);

  // Wait until the no audio page is not backgrounded and the audio page is
  // backgrounded.
  WaitUntilBackgrounded(no_audio_process_, false, audio_process_, true);
}

// Test to make sure that a process is backgrounded automatically when audio
// stops playing from a hidden tab.
IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostBackgroundingTestWithAudio,
                       ProcessPriorityAfterAudioStopsOnNotVisibleTab) {
  // This test is invalid on platforms that can't background.
  if (!base::Process::CanBackgroundProcesses())
    return;

  // Wait until the two pages are not backgrounded.
  WaitUntilBackgrounded(audio_process_, false, no_audio_process_, false);
  // Stop the audio.
  ASSERT_TRUE(content::ExecuteScript(
      audio_tab_web_contents_,
      "document.getElementById('audioPlayer').pause();"));

  // Wait until the no audio page is not backgrounded and the audio page is
  // backgrounded.
  WaitUntilBackgrounded(no_audio_process_, false, audio_process_, true);
}

// Test to make sure that a process is un-backgrounded automatically when
// audio
// starts playing from a backgrounded tab.
IN_PROC_BROWSER_TEST_F(ChromeRenderProcessHostBackgroundingTestWithAudio,
                       ProcessPriorityAfterAudioStartsFromBackgroundTab) {
  // This test is invalid on platforms that can't background.
  if (!base::Process::CanBackgroundProcesses())
    return;

  // Stop the audio.
  ASSERT_TRUE(content::ExecuteScript(
      audio_tab_web_contents_,
      "document.getElementById('audioPlayer').pause();"));

  WaitUntilBackgrounded(no_audio_process_, false, audio_process_, true);

  // Start the audio from the backgrounded tab.
  ASSERT_TRUE(
      content::ExecuteScript(audio_tab_web_contents_,
                             "document.getElementById('audioPlayer').play();"));

  // Wait until the two pages are not backgrounded.
  WaitUntilBackgrounded(no_audio_process_, false, audio_process_, false);
}
