// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_process_host_browsertest.h"

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/test_url_constants.h"
#include "content/public/common/content_switches.h"

RenderProcessHostTest::RenderProcessHostTest() {
  EnableDOMAutomation();
}

int RenderProcessHostTest::RenderProcessHostCount() {
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

IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, ProcessPerTab) {
  // Set max renderers to 1 to force running out of processes.
  content::RenderProcessHost::SetMaxRendererProcessCountForTest(1);

  CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  parsed_command_line.AppendSwitch(switches::kProcessPerTab);

  int tab_count = 1;
  int host_count = 1;

#if defined(TOUCH_UI)
  ++host_count;  // For the touch keyboard.
#endif

  // Change the first tab to be the new tab page (TYPE_WEBUI).
  GURL newtab(chrome::kTestNewTabURL);
  ui_test_utils::NavigateToURL(browser(), newtab);
  EXPECT_EQ(tab_count, browser()->tab_count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Create a new TYPE_TABBED tab.  It should be in its own process.
  GURL page1("data:text/html,hello world1");
  browser()->ShowSingletonTab(page1);
  if (browser()->tab_count() == tab_count)
    ui_test_utils::WaitForNewTab(browser());
  tab_count++;
  host_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Create another TYPE_TABBED tab.  It should share the previous process.
  GURL page2("data:text/html,hello world2");
  browser()->ShowSingletonTab(page2);
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

// When we hit the max number of renderers, verify that the way we do process
// sharing behaves correctly.  In particular, this test is verifying that even
// when we hit the max process limit, that renderers of each type will wind up
// in a process of that type, even if that means creating a new process.
IN_PROC_BROWSER_TEST_F(RenderProcessHostTest, ProcessOverflow) {
  // Set max renderers to 1 to force running out of processes.
  content::RenderProcessHost::SetMaxRendererProcessCountForTest(1);

  int tab_count = 1;
  int host_count = 1;
  TabContents* tab1 = NULL;
  TabContents* tab2 = NULL;
  content::RenderProcessHost* rph1 = NULL;
  content::RenderProcessHost* rph2 = NULL;
  content::RenderProcessHost* rph3 = NULL;

#if defined(TOUCH_UI)
  ++host_count;  // For the touch keyboard.
#endif

  // Change the first tab to be the new tab page (TYPE_WEBUI).
  GURL newtab(chrome::kTestNewTabURL);
  ui_test_utils::NavigateToURL(browser(), newtab);
  EXPECT_EQ(tab_count, browser()->tab_count());
  tab1 = browser()->GetTabContentsAt(tab_count - 1);
  rph1 = tab1->GetRenderProcessHost();
  EXPECT_EQ(tab1->GetURL(), newtab);
  EXPECT_EQ(host_count, RenderProcessHostCount());

  // Create a new TYPE_TABBED tab.  It should be in its own process.
  GURL page1("data:text/html,hello world1");
  browser()->ShowSingletonTab(page1);
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

  // Create another TYPE_TABBED tab.  It should share the previous process.
  GURL page2("data:text/html,hello world2");
  browser()->ShowSingletonTab(page2);
  if (browser()->tab_count() == tab_count)
    ui_test_utils::WaitForNewTab(browser());
  tab_count++;
  EXPECT_EQ(tab_count, browser()->tab_count());
  tab2 = browser()->GetTabContentsAt(tab_count - 1);
  EXPECT_EQ(tab2->GetURL(), page2);
  EXPECT_EQ(host_count, RenderProcessHostCount());
  EXPECT_EQ(tab2->GetRenderProcessHost(), rph2);

  // Create another TYPE_WEBUI tab.  It should share the process with newtab.
  // Note: intentionally create this tab after the TYPE_TABBED tabs to exercise
  // bug 43448 where extension and WebUI tabs could get combined into normal
  // renderers.
  GURL history(chrome::kTestHistoryURL);
  browser()->ShowSingletonTab(history);
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
  GURL bookmarks(chrome::kTestBookmarksURL);
  browser()->ShowSingletonTab(bookmarks);
  if (browser()->tab_count() == tab_count)
    ui_test_utils::WaitForNewTab(browser());
  tab_count++;
#if !defined(TOUCH_UI)
  // The keyboard in touchui already creates an extension process. So this
  // should not increase the process count.
  host_count++;
#endif
  EXPECT_EQ(tab_count, browser()->tab_count());
  tab1 = browser()->GetTabContentsAt(tab_count - 1);
  rph3 = tab1->GetRenderProcessHost();
  EXPECT_EQ(tab1->GetURL(), bookmarks);
  EXPECT_EQ(host_count, RenderProcessHostCount());
  EXPECT_NE(rph1, rph3);
  EXPECT_NE(rph2, rph3);
}
