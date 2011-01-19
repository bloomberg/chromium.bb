// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "ipc/ipc_message.h"

namespace {

class BrowserNavigatorTest : public InProcessBrowserTest,
                             public NotificationObserver {
 protected:
  GURL GetGoogleURL() const {
    return GURL("http://www.google.com/");
  }

  browser::NavigateParams MakeNavigateParams() const {
    return MakeNavigateParams(browser());
  }
  browser::NavigateParams MakeNavigateParams(Browser* browser) const {
    browser::NavigateParams params(browser, GetGoogleURL(),
                                   PageTransition::LINK);
    params.show_window = true;
    return params;
  }

  Browser* CreateEmptyBrowserForType(Browser::Type type, Profile* profile) {
    Browser* browser = Browser::CreateForType(type, profile);
    browser->AddBlankTab(true);
    return browser;
  }

  TabContentsWrapper* CreateTabContents() {
    return Browser::TabContentsFactory(
        browser()->profile(),
        NULL,
        MSG_ROUTING_NONE,
        browser()->GetSelectedTabContents(),
        NULL);
  }

  void RunSuppressTest(WindowOpenDisposition disposition) {
    GURL old_url = browser()->GetSelectedTabContents()->GetURL();
    browser::NavigateParams p(MakeNavigateParams());
    p.disposition = disposition;
    browser::Navigate(&p);

    // Nothing should have happened as a result of Navigate();
    EXPECT_EQ(1, browser()->tab_count());
    EXPECT_EQ(1u, BrowserList::size());
    EXPECT_EQ(old_url, browser()->GetSelectedTabContents()->GetURL());
  }

  void Observe(NotificationType type, const NotificationSource& source,
               const NotificationDetails& details) {
    switch (type.value) {
      case NotificationType::RENDER_VIEW_HOST_CREATED_FOR_TAB: {
        ++this->created_tab_contents_count_;
        break;
      }
      default:
        break;
    }
  }

  size_t created_tab_contents_count_;
};

// This test verifies that when a navigation occurs within a tab, the tab count
// of the Browser remains the same and the current tab bears the loaded URL.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_CurrentTab) {
  browser::NavigateParams p(MakeNavigateParams());
  browser::Navigate(&p);
  ui_test_utils::WaitForNavigationInCurrentTab(browser());
  EXPECT_EQ(GetGoogleURL(), browser()->GetSelectedTabContents()->GetURL());
  // We should have one window with one tab.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
}

// This test verifies that a singleton tab is refocused if one is already open
// in another or an existing window, or added if it is not.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_SingletonTabExisting) {
  GURL url("http://www.google.com/");
  GURL singleton_url1("http://maps.google.com/");

  // Register for a notification if an additional tab_contents was instantiated.
  // Opening a Singleton tab that is already open should not be opening a new
  // tab nor be creating a new TabContents object
  NotificationRegistrar registrar;

  // As the registrar object goes out of scope, this will get unregistered
  registrar.Add(this, NotificationType::RENDER_VIEW_HOST_CREATED_FOR_TAB,
                NotificationService::AllSources());

  browser()->AddSelectedTabWithURL(singleton_url1, PageTransition::LINK);
  browser()->AddSelectedTabWithURL(url, PageTransition::LINK);

  // We should have one browser with 3 tabs, the 3rd selected.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(2, browser()->selected_index());

  unsigned int previous_tab_contents_count =
      created_tab_contents_count_ = 0;

  // Navigate to singleton_url1.
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = singleton_url1;
  browser::Navigate(&p);

  // The middle tab should now be selected.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(1, browser()->selected_index());

  // No tab contents should have been created
  EXPECT_EQ(previous_tab_contents_count,
            created_tab_contents_count_);
}

IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabNoneExisting) {
  GURL url("http://www.google.com/");
  GURL singleton_url1("http://maps.google.com/");

  // We should have one browser with 3 tabs, the 3rd selected.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(0, browser()->selected_index());

  // Navigate to singleton_url1.
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = singleton_url1;
  browser::Navigate(&p);

  // We should now have 2 tabs, the 2nd one selected.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->selected_index());
}

// This test verifies that when a navigation results in a foreground tab, the
// tab count of the Browser increases and the selected tab shifts to the new
// foreground tab.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewForegroundTab) {
  TabContents* old_contents = browser()->GetSelectedTabContents();
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = NEW_FOREGROUND_TAB;
  browser::Navigate(&p);
  EXPECT_NE(old_contents, browser()->GetSelectedTabContents());
  EXPECT_EQ(browser()->GetSelectedTabContentsWrapper(), p.target_contents);
  EXPECT_EQ(2, browser()->tab_count());
}

// This test verifies that when a navigation results in a background tab, the
// tab count of the Browser increases but the selected tab remains the same.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewBackgroundTab) {
  TabContents* old_contents = browser()->GetSelectedTabContents();
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = NEW_BACKGROUND_TAB;
  browser::Navigate(&p);
  TabContents* new_contents = browser()->GetSelectedTabContents();
  // The selected tab should have remained unchanged, since the new tab was
  // opened in the background.
  EXPECT_EQ(old_contents, new_contents);
  EXPECT_EQ(2, browser()->tab_count());
}

// This test verifies that when a navigation requiring a new foreground tab
// occurs in a Browser that cannot host multiple tabs, the new foreground tab
// is created in an existing compatible Browser.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_IncompatibleWindow_Existing) {
  // Open a foreground tab in a window that cannot open popups when there is an
  // existing compatible window somewhere else that they can be opened within.
  Browser* popup = CreateEmptyBrowserForType(Browser::TYPE_POPUP,
                                             browser()->profile());
  browser::NavigateParams p(MakeNavigateParams(popup));
  p.disposition = NEW_FOREGROUND_TAB;
  browser::Navigate(&p);

  // Navigate() should have opened the tab in a different browser since the
  // one we supplied didn't support additional tabs.
  EXPECT_NE(popup, p.browser);

  // Since browser() is an existing compatible tabbed browser, it should have
  // opened the tab there.
  EXPECT_EQ(browser(), p.browser);

  // We should be left with 2 windows, the popup with one tab and the browser()
  // provided by the framework with two.
  EXPECT_EQ(2u, BrowserList::size());
  EXPECT_EQ(1, popup->tab_count());
  EXPECT_EQ(2, browser()->tab_count());
}

// This test verifies that when a navigation requiring a new foreground tab
// occurs in a Browser that cannot host multiple tabs and no compatible Browser
// that can is open, a compatible Browser is created.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_IncompatibleWindow_NoExisting) {
  // We want to simulate not being able to find an existing window compatible
  // with our non-tabbed browser window so Navigate() is forced to create a
  // new compatible window. Because browser() supplied by the in-process
  // browser testing framework is compatible with browser()->profile(), we
  // need a different profile, and creating a popup window with an incognito
  // profile is a quick and dirty way of achieving this.
  Browser* popup = CreateEmptyBrowserForType(
      Browser::TYPE_POPUP, browser()->profile()->GetOffTheRecordProfile());
  browser::NavigateParams p(MakeNavigateParams(popup));
  p.disposition = NEW_FOREGROUND_TAB;
  browser::Navigate(&p);

  // Navigate() should have opened the tab in a different browser since the
  // one we supplied didn't support additional tabs.
  EXPECT_NE(popup, p.browser);

  // This time, browser() is _not_ compatible with popup since it is not an
  // incognito window.
  EXPECT_NE(browser(), p.browser);

  // We should have three windows, each with one tab:
  // 1. the browser() provided by the framework (unchanged in this test)
  // 2. the incognito popup we created originally
  // 3. the new incognito tabbed browser that was created by Navigate().
  EXPECT_EQ(3u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1, popup->tab_count());
  EXPECT_EQ(1, p.browser->tab_count());
  EXPECT_EQ(Browser::TYPE_NORMAL, p.browser->type());
}

// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// from a normal Browser results in a new Browser with TYPE_POPUP.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewPopup) {
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = NEW_POPUP;
  browser::Navigate(&p);

  // Navigate() should have opened a new popup window.
  EXPECT_NE(browser(), p.browser);
  EXPECT_EQ(Browser::TYPE_POPUP, p.browser->type());

  // We should have two windows, the browser() provided by the framework and the
  // new popup window.
  EXPECT_EQ(2u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1, p.browser->tab_count());
}

// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// from a normal popup results in a new Browser with TYPE_POPUP.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewPopupFromPopup) {
  // Open a popup.
  browser::NavigateParams p1(MakeNavigateParams());
  p1.disposition = NEW_POPUP;
  browser::Navigate(&p1);
  // Open another popup.
  browser::NavigateParams p2(MakeNavigateParams(p1.browser));
  p2.disposition = NEW_POPUP;
  browser::Navigate(&p2);

  // Navigate() should have opened a new normal popup window.
  EXPECT_NE(p1.browser, p2.browser);
  EXPECT_EQ(Browser::TYPE_POPUP, p2.browser->type());

  // We should have three windows, the browser() provided by the framework,
  // the first popup window, and the second popup window.
  EXPECT_EQ(3u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1, p1.browser->tab_count());
  EXPECT_EQ(1, p2.browser->tab_count());
}

// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// from an app frame results in a new Browser with TYPE_APP_POPUP.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_NewPopupFromAppWindow) {
  Browser* app_browser = CreateEmptyBrowserForType(Browser::TYPE_APP,
                                                   browser()->profile());
  browser::NavigateParams p(MakeNavigateParams(app_browser));
  p.disposition = NEW_POPUP;
  browser::Navigate(&p);

  // Navigate() should have opened a new popup app window.
  EXPECT_NE(app_browser, p.browser);
  EXPECT_NE(browser(), p.browser);
  EXPECT_EQ(Browser::TYPE_APP_POPUP, p.browser->type());

  // We should now have three windows, the app window, the app popup it created,
  // and the original browser() provided by the framework.
  EXPECT_EQ(3u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1, app_browser->tab_count());
  EXPECT_EQ(1, p.browser->tab_count());
}

// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// from an app popup results in a new Browser also of TYPE_APP_POPUP.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_NewPopupFromAppPopup) {
  Browser* app_browser = CreateEmptyBrowserForType(Browser::TYPE_APP,
                                                   browser()->profile());
  // Open an app popup.
  browser::NavigateParams p1(MakeNavigateParams(app_browser));
  p1.disposition = NEW_POPUP;
  browser::Navigate(&p1);
  // Now open another app popup.
  browser::NavigateParams p2(MakeNavigateParams(p1.browser));
  p2.disposition = NEW_POPUP;
  browser::Navigate(&p2);

  // Navigate() should have opened a new popup app window.
  EXPECT_NE(browser(), p1.browser);
  EXPECT_NE(p1.browser, p2.browser);
  EXPECT_EQ(Browser::TYPE_APP_POPUP, p2.browser->type());

  // We should now have four windows, the app window, the first app popup,
  // the second app popup, and the original browser() provided by the framework.
  EXPECT_EQ(4u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1, app_browser->tab_count());
  EXPECT_EQ(1, p1.browser->tab_count());
  EXPECT_EQ(1, p2.browser->tab_count());
}

// This test verifies that navigating with WindowOpenDisposition = NEW_POPUP
// from an extension app tab results in a new Browser with TYPE_APP_POPUP.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_NewPopupFromExtensionApp) {
  // TODO(beng): TBD.
}

// This test verifies that navigating with WindowOpenDisposition = NEW_WINDOW
// always opens a new window.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_NewWindow) {
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = NEW_WINDOW;
  browser::Navigate(&p);

  // Navigate() should have opened a new toplevel window.
  EXPECT_NE(browser(), p.browser);
  EXPECT_EQ(Browser::TYPE_NORMAL, p.browser->type());

  // We should now have two windows, the browser() provided by the framework and
  // the new normal window.
  EXPECT_EQ(2u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1, p.browser->tab_count());
}

// This test verifies that navigating with WindowOpenDisposition = INCOGNITO
// opens a new incognito window if no existing incognito window is present.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_Incognito) {
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = OFF_THE_RECORD;
  browser::Navigate(&p);

  // Navigate() should have opened a new toplevel incognito window.
  EXPECT_NE(browser(), p.browser);
  EXPECT_EQ(browser()->profile()->GetOffTheRecordProfile(),
            p.browser->profile());

  // |source_contents| should be set to NULL because the profile for the new
  // page is different from the originating page.
  EXPECT_EQ(NULL, p.source_contents);

  // We should now have two windows, the browser() provided by the framework and
  // the new incognito window.
  EXPECT_EQ(2u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1, p.browser->tab_count());
}

// This test verifies that navigating with WindowOpenDisposition = INCOGNITO
// reuses an existing incognito window when possible.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_IncognitoRefocus) {
  Browser* incognito_browser =
      CreateEmptyBrowserForType(Browser::TYPE_NORMAL,
                                browser()->profile()->GetOffTheRecordProfile());
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = OFF_THE_RECORD;
  browser::Navigate(&p);

  // Navigate() should have opened a new tab in the existing incognito window.
  EXPECT_NE(browser(), p.browser);
  EXPECT_EQ(p.browser, incognito_browser);

  // We should now have two windows, the browser() provided by the framework and
  // the incognito window we opened earlier.
  EXPECT_EQ(2u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(2, incognito_browser->tab_count());
}

// This test verifies that no navigation action occurs when
// WindowOpenDisposition = SUPPRESS_OPEN.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_SuppressOpen) {
  RunSuppressTest(SUPPRESS_OPEN);
}

// This test verifies that no navigation action occurs when
// WindowOpenDisposition = SAVE_TO_DISK.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_SaveToDisk) {
  RunSuppressTest(SAVE_TO_DISK);
}

// This test verifies that no navigation action occurs when
// WindowOpenDisposition = IGNORE_ACTION.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Disposition_IgnoreAction) {
  RunSuppressTest(IGNORE_ACTION);
}

// This tests adding a foreground tab with a predefined TabContents.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, TargetContents_ForegroundTab) {
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = NEW_FOREGROUND_TAB;
  p.target_contents = CreateTabContents();
  browser::Navigate(&p);

  // Navigate() should have opened the contents in a new foreground in the
  // current Browser.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(browser()->GetSelectedTabContentsWrapper(), p.target_contents);

  // We should have one window, with two tabs.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(2, browser()->tab_count());
}

#if defined(OS_WIN)
// This tests adding a popup with a predefined TabContents.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, TargetContents_Popup) {
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = NEW_POPUP;
  p.target_contents = CreateTabContents();
  p.window_bounds = gfx::Rect(10, 10, 500, 500);
  browser::Navigate(&p);

  // Navigate() should have opened a new popup window.
  EXPECT_NE(browser(), p.browser);
  EXPECT_EQ(Browser::TYPE_POPUP, p.browser->type());

  // The web platform is weird. The window bounds specified in
  // |p.window_bounds| are used as follows:
  // - the origin is used to position the window
  // - the size is used to size the TabContents of the window.
  // As such the position of the resulting window will always match
  // p.window_bounds.origin(), but its size will not. We need to match
  // the size against the selected tab's view's container size.
  // Only Windows positions the window according to |p.window_bounds.origin()| -
  // on Mac the window is offset from the opener and on Linux it always opens
  // at 0,0.
  EXPECT_EQ(p.window_bounds.origin(),
            p.browser->window()->GetRestoredBounds().origin());
  // All platforms should respect size however provided width > 400 (Mac has a
  // minimum window width of 400).
  EXPECT_EQ(p.window_bounds.size(),
            p.target_contents->tab_contents()->view()->GetContainerSize());

  // We should have two windows, the new popup and the browser() provided by the
  // framework.
  EXPECT_EQ(2u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1, p.browser->tab_count());
}
#endif

// This tests adding a tab at a specific index.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, Tabstrip_InsertAtIndex) {
  // This is not meant to be a comprehensive test of whether or not the tab
  // implementation of the browser observes the insertion index. That is
  // covered by the unit tests for TabStripModel. This merely verifies that
  // insertion index preference is reflected in common cases.
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = NEW_FOREGROUND_TAB;
  p.tabstrip_index = 0;
  p.tabstrip_add_types = TabStripModel::ADD_FORCE_INDEX;
  browser::Navigate(&p);

  // Navigate() should have inserted a new tab at slot 0 in the tabstrip.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(0, browser()->tabstrip_model()->GetIndexOfTabContents(
      static_cast<const TabContentsWrapper*>(p.target_contents)));

  // We should have one window - the browser() provided by the framework.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(2, browser()->tab_count());
}

// This test verifies that constructing params with a NULL browser has
// the same result as navigating to a new foreground tab in the (only)
// active browser. Tests are the same as for Disposition_NewForegroundTab.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, NullBrowser_NewForegroundTab) {
  TabContents* old_contents = browser()->GetSelectedTabContents();
  // Navigate with a NULL browser.
  browser::NavigateParams p(MakeNavigateParams(NULL));
  p.disposition = NEW_FOREGROUND_TAB;
  p.profile = browser()->profile();
  browser::Navigate(&p);

  // Navigate() should have found browser() and create a new tab.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_NE(old_contents, browser()->GetSelectedTabContents());
  EXPECT_EQ(browser()->GetSelectedTabContentsWrapper(), p.target_contents);
  EXPECT_EQ(2, browser()->tab_count());
}

// This test verifies that constructing params with a NULL browser and
// a specific profile matches the specified profile.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, NullBrowser_MatchProfile) {
  // Create a new browser with using the incognito profile.
  Browser* incognito =
      Browser::Create(browser()->profile()->GetOffTheRecordProfile());

  // Navigate with a NULL browser and the incognito profile.
  browser::NavigateParams p(MakeNavigateParams(NULL));
  p.disposition = NEW_FOREGROUND_TAB;
  p.profile = incognito->profile();
  browser::Navigate(&p);

  // Navigate() should have found incognito, not browser().
  EXPECT_EQ(incognito, p.browser);
  EXPECT_EQ(incognito->GetSelectedTabContentsWrapper(), p.target_contents);
  EXPECT_EQ(1, incognito->tab_count());
}

// This test verifies that constructing params with a NULL browser and
// disposition = NEW_WINDOW always opens exactly one new window.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest, NullBrowser_NewWindow) {
  browser::NavigateParams p(MakeNavigateParams(NULL));
  p.disposition = NEW_WINDOW;
  p.profile = browser()->profile();
  browser::Navigate(&p);

  // Navigate() should have created a new browser.
  EXPECT_NE(browser(), p.browser);
  EXPECT_EQ(Browser::TYPE_NORMAL, p.browser->type());

  // We should now have two windows, the browser() provided by the framework and
  // the new normal window.
  EXPECT_EQ(2u, BrowserList::size());
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1, p.browser->tab_count());
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and |ignore_paths| = true opens a new tab navigated to the specified URL if
// no previous tab with that URL (minus the path) exists.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabNew_IgnorePath) {
  GURL url("http://www.google.com/");
  browser()->AddSelectedTabWithURL(url, PageTransition::LINK);

  // We should have one browser with 2 tabs, the 2nd selected.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->selected_index());

  // Navigate to a new singleton tab with a sub-page.
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = GURL("chrome://settings/advanced");
  p.show_window = true;
  p.ignore_path = true;
  browser::Navigate(&p);

  // The last tab should now be selected and navigated to the sub-page of the
  // URL.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_EQ(2, browser()->selected_index());
  EXPECT_EQ(GURL("chrome://settings/advanced"),
            browser()->GetSelectedTabContents()->GetURL());
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and |ignore_paths| = true opens an existing tab with the matching URL (minus
// the path) which is navigated to the specified URL.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabExisting_IgnorePath) {
  GURL singleton_url1("chrome://settings");
  GURL url("http://www.google.com/");
  browser()->AddSelectedTabWithURL(singleton_url1, PageTransition::LINK);
  browser()->AddSelectedTabWithURL(url, PageTransition::LINK);

  // We should have one browser with 3 tabs, the 3rd selected.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_EQ(2, browser()->selected_index());

  // Navigate to singleton_url1.
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = GURL("chrome://settings/advanced");
  p.show_window = true;
  p.ignore_path = true;
  browser::Navigate(&p);

  // The middle tab should now be selected and navigated to the sub-page of the
  // URL.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_EQ(1, browser()->selected_index());
  EXPECT_EQ(GURL("chrome://settings/advanced"),
            browser()->GetSelectedTabContents()->GetURL());
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and |ignore_paths| = true opens an existing tab with the matching URL (minus
// the path) which is navigated to the specified URL.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabExistingSubPath_IgnorePath) {
  GURL singleton_url1("chrome://settings/advanced");
  GURL url("http://www.google.com/");
  browser()->AddSelectedTabWithURL(singleton_url1, PageTransition::LINK);
  browser()->AddSelectedTabWithURL(url, PageTransition::LINK);

  // We should have one browser with 3 tabs, the 3rd selected.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_EQ(2, browser()->selected_index());

  // Navigate to singleton_url1.
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = GURL("chrome://settings/personal");
  p.show_window = true;
  p.ignore_path = true;
  browser::Navigate(&p);

  // The middle tab should now be selected and navigated to the sub-page of the
  // URL.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(3, browser()->tab_count());
  EXPECT_EQ(1, browser()->selected_index());
  EXPECT_EQ(GURL("chrome://settings/personal"),
            browser()->GetSelectedTabContents()->GetURL());
}

// This test verifies that constructing params with disposition = SINGLETON_TAB
// and |ignore_paths| = true will update the current tab's URL if the currently
// selected tab is a match but has a different path.
IN_PROC_BROWSER_TEST_F(BrowserNavigatorTest,
                       Disposition_SingletonTabFocused_IgnorePath) {
  GURL singleton_url_current("chrome://settings/advanced");
  GURL url("http://www.google.com/");
  browser()->AddSelectedTabWithURL(singleton_url_current, PageTransition::LINK);

  // We should have one browser with 2 tabs, the 2nd selected.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->selected_index());

  // Navigate to a different settings path.
  GURL singleton_url_target("chrome://settings/personal");
  browser::NavigateParams p(MakeNavigateParams());
  p.disposition = SINGLETON_TAB;
  p.url = singleton_url_target;
  p.show_window = true;
  p.ignore_path = true;
  browser::Navigate(&p);

  // The second tab should still be selected, but navigated to the new path.
  EXPECT_EQ(browser(), p.browser);
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->selected_index());
  EXPECT_EQ(singleton_url_target,
            browser()->GetSelectedTabContents()->GetURL());
}

}  // namespace
