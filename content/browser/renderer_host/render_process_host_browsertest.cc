// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/test_url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"

using content::WebContents;

void PostQuit(MessageLoop* loop) {
  loop->PostTask(FROM_HERE, MessageLoop::QuitClosure());
}

class RenderProcessHostTest : public InProcessBrowserTest {
 public:
  RenderProcessHostTest() {
    EnableDOMAutomation();
  }

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

  // Show a tab, activating the current one if there is one, and wait for
  // the renderer process to be created or foregrounded, returning the process
  // handle.
  base::ProcessHandle ShowSingletonTab(const GURL& page) {
    chrome::ShowSingletonTab(browser(), page);
    WebContents* wc = browser()->GetActiveWebContents();
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
    GURL newtab(chrome::kTestNewTabURL);
    ui_test_utils::NavigateToURL(browser(), newtab);
    EXPECT_EQ(tab_count, browser()->tab_count());
    tab1 = browser()->GetWebContentsAt(tab_count - 1);
    rph1 = tab1->GetRenderProcessHost();
    EXPECT_EQ(tab1->GetURL(), newtab);
    EXPECT_EQ(host_count, RenderProcessHostCount());

    // Create a new TYPE_TABBED tab.  It should be in its own process.
    GURL page1("data:text/html,hello world1");
    chrome::ShowSingletonTab(browser(), page1);
    if (browser()->tab_count() == tab_count)
      ui_test_utils::WaitForNewTab(browser());
    tab_count++;
    host_count++;
    EXPECT_EQ(tab_count, browser()->tab_count());
    tab1 = browser()->GetWebContentsAt(tab_count - 1);
    rph2 = tab1->GetRenderProcessHost();
    EXPECT_EQ(tab1->GetURL(), page1);
    EXPECT_EQ(host_count, RenderProcessHostCount());
    EXPECT_NE(rph1, rph2);

    // Create another TYPE_TABBED tab.  It should share the previous process.
    GURL page2("data:text/html,hello world2");
    chrome::ShowSingletonTab(browser(), page2);
    if (browser()->tab_count() == tab_count)
      ui_test_utils::WaitForNewTab(browser());
    tab_count++;
    EXPECT_EQ(tab_count, browser()->tab_count());
    tab2 = browser()->GetWebContentsAt(tab_count - 1);
    EXPECT_EQ(tab2->GetURL(), page2);
    EXPECT_EQ(host_count, RenderProcessHostCount());
    EXPECT_EQ(tab2->GetRenderProcessHost(), rph2);

    // Create another TYPE_WEBUI tab.  It should share the process with newtab.
    // Note: intentionally create this tab after the TYPE_TABBED tabs to
    // exercise bug 43448 where extension and WebUI tabs could get combined into
    // normal renderers.
    GURL history(chrome::kTestHistoryURL);
    chrome::ShowSingletonTab(browser(), history);
    if (browser()->tab_count() == tab_count)
      ui_test_utils::WaitForNewTab(browser());
    tab_count++;
    EXPECT_EQ(tab_count, browser()->tab_count());
    tab2 = browser()->GetWebContentsAt(tab_count - 1);
    EXPECT_EQ(tab2->GetURL(), history);
    EXPECT_EQ(host_count, RenderProcessHostCount());
    EXPECT_EQ(tab2->GetRenderProcessHost(), rph1);

    // Create a TYPE_EXTENSION tab.  It should be in its own process.
    // (the bookmark manager is implemented as an extension)
    GURL bookmarks(chrome::kTestBookmarksURL);
    chrome::ShowSingletonTab(browser(), bookmarks);
    if (browser()->tab_count() == tab_count)
      ui_test_utils::WaitForNewTab(browser());
    tab_count++;
    host_count++;
    EXPECT_EQ(tab_count, browser()->tab_count());
    tab1 = browser()->GetWebContentsAt(tab_count - 1);
    rph3 = tab1->GetRenderProcessHost();
    EXPECT_EQ(tab1->GetURL(), bookmarks);
    EXPECT_EQ(host_count, RenderProcessHostCount());
    EXPECT_NE(rph1, rph3);
    EXPECT_NE(rph2, rph3);
  }
};


class RenderProcessHostTestWithCommandLine : public RenderProcessHostTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kRendererProcessLimit, "1");
  }
};

IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, ProcessPerTab) {
  // Set max renderers to 1 to force running out of processes.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  parsed_command_line.AppendSwitch(switches::kProcessPerTab);

  int tab_count = 1;
  int host_count = 1;

  // Change the first tab to be the new tab page (TYPE_WEBUI).
  GURL newtab(chrome::kTestNewTabURL);
  ui_test_utils::NavigateToURL(browser(), newtab);
  EXPECT_EQ(tab_count, browser()->tab_count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Create a new TYPE_TABBED tab.  It should be in its own process.
  GURL page1("data:text/html,hello world1");
  chrome::ShowSingletonTab(browser(), page1);
  if (browser()->tab_count() == tab_count)
    ui_test_utils::WaitForNewTab(browser());
  tab_count++;
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Create another TYPE_TABBED tab.  It should share the previous process.
  GURL page2("data:text/html,hello world2");
  chrome::ShowSingletonTab(browser(), page2);
  if (browser()->tab_count() == tab_count)
    ui_test_utils::WaitForNewTab(browser());
  tab_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Create another new tab.  It should share the process with the other WebUI.
  chrome::NewTab(browser());
  if (browser()->tab_count() == tab_count)
    ui_test_utils::WaitForNewTab(browser());
  tab_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Create another new tab.  It should share the process with the other WebUI.
  chrome::NewTab(browser());
  if (browser()->tab_count() == tab_count)
    ui_test_utils::WaitForNewTab(browser());
  tab_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  EXPECT_EQ(host_count, RenderProcessHostCount());
}

// We don't change process priorities on Mac or Posix because the user lacks the
// permission to raise a process' priority even after lowering it.
#if defined(OS_WIN) || defined(OS_LINUX)
IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, Backgrounding) {
  if (!base::Process::CanBackgroundProcesses()) {
    LOG(ERROR) << "Can't background processes";
    return;
  }
  CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  parsed_command_line.AppendSwitch(switches::kProcessPerTab);

  // Change the first tab to be the new tab page (TYPE_WEBUI).
  GURL newtab(chrome::kTestNewTabURL);
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

IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, ProcessOverflow) {
  // Set max renderers to 1 to force running out of processes.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);
  TestProcessOverflow();
}

// Variation of the ProcessOverflow test, which is driven through command line
// parameter instead of direct function call into the class.
IN_PROC_BROWSER_TEST_F(RenderProcessHostTestWithCommandLine, ProcessOverflow) {
  TestProcessOverflow();
}
