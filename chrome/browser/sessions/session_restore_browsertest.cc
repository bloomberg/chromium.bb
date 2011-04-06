// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/page_transition_types.h"

namespace {

// BrowserList::Observer implementation that waits for a browser to be
// removed.
class BrowserListObserverImpl : public BrowserList::Observer {
 public:
  BrowserListObserverImpl() : did_remove_(false), running_(false) {
    BrowserList::AddObserver(this);
  }

  ~BrowserListObserverImpl() {
    BrowserList::RemoveObserver(this);
  }

  // Returns when a browser has been removed.
  void Run() {
    running_ = true;
    if (!did_remove_)
      ui_test_utils::RunMessageLoop();
  }

  // BrowserList::Observer
  virtual void OnBrowserAdded(const Browser* browser) OVERRIDE {
  }
  virtual void OnBrowserRemoved(const Browser* browser) OVERRIDE {
    did_remove_ = true;
    if (running_)
      MessageLoop::current()->Quit();
  }

 private:
  // Was OnBrowserRemoved invoked?
  bool did_remove_;

  // Was Run invoked?
  bool running_;

  DISALLOW_COPY_AND_ASSIGN(BrowserListObserverImpl);
};

}  // namespace

typedef InProcessBrowserTest SessionRestoreTest;

#if defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
// Crashes on Linux Views: http://crbug.com/39476
#define MAYBE_RestoreOnNewWindowWithNoTabbedBrowsers \
        DISABLED_RestoreOnNewWindowWithNoTabbedBrowsers
#else
#define MAYBE_RestoreOnNewWindowWithNoTabbedBrowsers \
        RestoreOnNewWindowWithNoTabbedBrowsers
#endif

// Makes sure when session restore is triggered in the same process we don't end
// up with an extra tab.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest,
                       MAYBE_RestoreOnNewWindowWithNoTabbedBrowsers) {
  if (browser_defaults::kRestorePopups)
    return;

  const FilePath::CharType* kTitle1File = FILE_PATH_LITERAL("title1.html");
  GURL url(ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                     FilePath(kTitle1File)));
  ui_test_utils::NavigateToURL(browser(), url);

  // Turn on session restore.
  SessionStartupPref::SetStartupPref(
      browser()->profile(),
      SessionStartupPref(SessionStartupPref::LAST));

  // Create a new popup.
  Profile* profile = browser()->profile();
  Browser* popup = Browser::CreateForType(Browser::TYPE_POPUP, profile);
  popup->window()->Show();

  // Close the browser.
  browser()->window()->Close();

  // Create a new window, which should trigger session restore.
  popup->NewWindow();

  Browser* new_browser = ui_test_utils::WaitForNewBrowser();

  ASSERT_TRUE(new_browser != NULL);

  // The browser should only have one tab.
  ASSERT_EQ(1, new_browser->tab_count());

  // And the first url should be url.
  EXPECT_EQ(url, new_browser->GetTabContentsAt(0)->GetURL());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestoreIndividualTabFromWindow) {
  GURL url1(ui_test_utils::GetTestUrl(
      FilePath(FilePath::kCurrentDirectory),
      FilePath(FILE_PATH_LITERAL("title1.html"))));
  GURL url2(ui_test_utils::GetTestUrl(
      FilePath(FilePath::kCurrentDirectory),
      FilePath(FILE_PATH_LITERAL("title2.html"))));
  GURL url3(ui_test_utils::GetTestUrl(
      FilePath(FilePath::kCurrentDirectory),
      FilePath(FILE_PATH_LITERAL("title3.html"))));

  // Add and navigate three tabs.
  ui_test_utils::NavigateToURL(browser(), url1);
  browser()->AddSelectedTabWithURL(url2, PageTransition::LINK);
  ui_test_utils::WaitForNavigationInCurrentTab(browser());

  browser()->AddSelectedTabWithURL(url3, PageTransition::LINK);
  ui_test_utils::WaitForNavigationInCurrentTab(browser());

  TabRestoreService* service = browser()->profile()->GetTabRestoreService();
  service->ClearEntries();

  browser()->window()->Close();

  // Expect a window with three tabs.
  EXPECT_EQ(1U, service->entries().size());
  ASSERT_EQ(TabRestoreService::WINDOW, service->entries().front()->type);
  const TabRestoreService::Window* window =
      static_cast<TabRestoreService::Window*>(service->entries().front());
  EXPECT_EQ(3U, window->tabs.size());

  // Find the SessionID for entry2. Since the session service was destroyed,
  // there is no guarantee that the SessionID for the tab has remained the same.
  std::vector<TabRestoreService::Tab>::const_iterator it = window->tabs.begin();
  for ( ; it != window->tabs.end(); ++it) {
    const TabRestoreService::Tab& tab = *it;
    // If this tab held url2, then restore this single tab.
    if (tab.navigations[0].virtual_url() == url2) {
      service->RestoreEntryById(NULL, tab.id, false);
      break;
    }
  }

  // Make sure that the Window got updated.
  EXPECT_EQ(1U, service->entries().size());
  ASSERT_EQ(TabRestoreService::WINDOW, service->entries().front()->type);
  window = static_cast<TabRestoreService::Window*>(service->entries().front());
  EXPECT_EQ(2U, window->tabs.size());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, WindowWithOneTab) {
  GURL url(ui_test_utils::GetTestUrl(
      FilePath(FilePath::kCurrentDirectory),
      FilePath(FILE_PATH_LITERAL("title1.html"))));

  // Add a single tab.
  ui_test_utils::NavigateToURL(browser(), url);

  TabRestoreService* service = browser()->profile()->GetTabRestoreService();
  service->ClearEntries();
  EXPECT_EQ(0U, service->entries().size());

  // Close the window.
  browser()->window()->Close();

  // Expect the window to be converted to a tab by the TRS.
  EXPECT_EQ(1U, service->entries().size());
  ASSERT_EQ(TabRestoreService::TAB, service->entries().front()->type);
  const TabRestoreService::Tab* tab =
      static_cast<TabRestoreService::Tab*>(service->entries().front());

  // Restore the tab.
  service->RestoreEntryById(NULL, tab->id, false);

  // Make sure the restore was successful.
  EXPECT_EQ(0U, service->entries().size());
}

// Verifies we remember the last browser window when closing the last
// non-incognito window while an incognito window is open.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, IncognitotoNonIncognito) {
  // Turn on session restore.
  SessionStartupPref pref(SessionStartupPref::LAST);
  SessionStartupPref::SetStartupPref(browser()->profile(), pref);

  GURL url(ui_test_utils::GetTestUrl(
      FilePath(FilePath::kCurrentDirectory),
      FilePath(FILE_PATH_LITERAL("title1.html"))));

  // Add a single tab.
  ui_test_utils::NavigateToURL(browser(), url);

  // Create a new incognito window.
  Browser* incognito_browser = CreateIncognitoBrowser();
  incognito_browser->AddBlankTab(true);
  incognito_browser->window()->Show();

  // Close the normal browser. After this we only have the incognito window
  // open. We wait until the window closes as window closing is async.
  {
    BrowserListObserverImpl observer;
    browser()->window()->Close();
    observer.Run();
  }

  // Create a new window, which should trigger session restore.
  incognito_browser->NewWindow();

  // The first tab should have 'url' as its url.
  Browser* new_browser = ui_test_utils::WaitForNewBrowser();
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(url, new_browser->GetTabContentsAt(0)->GetURL());
}
