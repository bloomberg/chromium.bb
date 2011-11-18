// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif
#include "base/sys_info.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_helper.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tabs/pinned_tab_codec.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/translate/translate_tab_helper.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog.h"
#include "chrome/browser/ui/app_modal_dialogs/js_modal_dialog.h"
#include "chrome/browser/ui/app_modal_dialogs/native_app_modal_dialog.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_init.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/fullscreen_exit_bubble_type.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/mock_host_resolver.h"
#include "net/test/test_server.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "base/i18n/rtl.h"
#include "chrome/browser/browser_process.h"
#endif

namespace {

const char* kBeforeUnloadHTML =
    "<html><head><title>beforeunload</title></head><body>"
    "<script>window.onbeforeunload=function(e){return 'foo'}</script>"
    "</body></html>";

const char* kOpenNewBeforeUnloadPage =
    "w=window.open(); w.onbeforeunload=function(e){return 'foo'};";

const FilePath::CharType* kBeforeUnloadFile =
    FILE_PATH_LITERAL("beforeunload.html");

const FilePath::CharType* kTitle1File = FILE_PATH_LITERAL("title1.html");
const FilePath::CharType* kTitle2File = FILE_PATH_LITERAL("title2.html");

const FilePath::CharType kDocRoot[] = FILE_PATH_LITERAL("chrome/test/data");

// Given a page title, returns the expected window caption string.
std::wstring WindowCaptionFromPageTitle(std::wstring page_title) {
#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
  // On Mac or ChromeOS, we don't want to suffix the page title with
  // the application name.
  if (page_title.empty()) {
    return UTF16ToWideHack(
        l10n_util::GetStringUTF16(IDS_BROWSER_WINDOW_MAC_TAB_UNTITLED));
  }
  return page_title;
#else
  if (page_title.empty())
    return UTF16ToWideHack(l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));

  return UTF16ToWideHack(
      l10n_util::GetStringFUTF16(IDS_BROWSER_WINDOW_TITLE_FORMAT,
                                 WideToUTF16Hack(page_title)));
#endif
}

// Returns the number of active RenderProcessHosts.
int CountRenderProcessHosts() {
  int result = 0;
  for (content::RenderProcessHost::iterator i(
          content::RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance())
    ++result;
  return result;
}

class MockTabStripModelObserver : public TabStripModelObserver {
 public:
  MockTabStripModelObserver() : closing_count_(0) {}

  virtual void TabClosingAt(TabStripModel* tab_strip_model,
                            TabContentsWrapper* contents,
                            int index) {
    closing_count_++;
  }

  int closing_count() const { return closing_count_; }

 private:
  int closing_count_;

  DISALLOW_COPY_AND_ASSIGN(MockTabStripModelObserver);
};

// Used by CloseWithAppMenuOpen. Invokes CloseWindow on the supplied browser.
class CloseWindowTask : public Task {
 public:
  explicit CloseWindowTask(Browser* browser) : browser_(browser) {}

  virtual void Run() {
    browser_->CloseWindow();
  }

 private:
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(CloseWindowTask);
};

// Used by CloseWithAppMenuOpen. Posts a CloseWindowTask and shows the app menu.
class RunCloseWithAppMenuTask : public Task {
 public:
  explicit RunCloseWithAppMenuTask(Browser* browser) : browser_(browser) {}

  virtual void Run() {
    // ShowAppMenu is modal under views. Schedule a task that closes the window.
    MessageLoop::current()->PostTask(FROM_HERE, new CloseWindowTask(browser_));
    browser_->ShowAppMenu();
  }

 private:
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(RunCloseWithAppMenuTask);
};

}  // namespace

class BrowserTest : public ExtensionBrowserTest {
 protected:
  // In RTL locales wrap the page title with RTL embedding characters so that it
  // matches the value returned by GetWindowTitle().
  std::wstring LocaleWindowCaptionFromPageTitle(
      const std::wstring& expected_title) {
    std::wstring page_title = WindowCaptionFromPageTitle(expected_title);
#if defined(OS_WIN)
    std::string locale = g_browser_process->GetApplicationLocale();
    if (base::i18n::GetTextDirectionForLocale(locale.c_str()) ==
        base::i18n::RIGHT_TO_LEFT) {
      base::i18n::WrapStringWithLTRFormatting(&page_title);
    }

    return page_title;
#else
    // Do we need to use the above code on POSIX as well?
    return page_title;
#endif
  }

  // Returns the app extension aptly named "App Test".
  const Extension* GetExtension() {
    const ExtensionList* extensions =
        browser()->profile()->GetExtensionService()->extensions();
    for (size_t i = 0; i < extensions->size(); ++i) {
      if ((*extensions)[i]->name() == "App Test")
        return (*extensions)[i];
    }
    NOTREACHED();
    return NULL;
  }
};

// Launch the app on a page with no title, check that the app title was set
// correctly.
IN_PROC_BROWSER_TEST_F(BrowserTest, NoTitle) {
  ui_test_utils::NavigateToURL(browser(),
      ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                FilePath(kTitle1File)));
  EXPECT_EQ(LocaleWindowCaptionFromPageTitle(L"title1.html"),
            UTF16ToWideHack(browser()->GetWindowTitleForCurrentTab()));
  string16 tab_title;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &tab_title));
  EXPECT_EQ(ASCIIToUTF16("title1.html"), tab_title);
}

// Launch the app, navigate to a page with a title, check that the app title
// was set correctly.
IN_PROC_BROWSER_TEST_F(BrowserTest, Title) {
  ui_test_utils::NavigateToURL(browser(),
      ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                FilePath(kTitle2File)));
  const std::wstring test_title(L"Title Of Awesomeness");
  EXPECT_EQ(LocaleWindowCaptionFromPageTitle(test_title),
            UTF16ToWideHack(browser()->GetWindowTitleForCurrentTab()));
  string16 tab_title;
  ASSERT_TRUE(ui_test_utils::GetCurrentTabTitle(browser(), &tab_title));
  EXPECT_EQ(WideToUTF16(test_title), tab_title);
}

IN_PROC_BROWSER_TEST_F(BrowserTest, JavascriptAlertActivatesTab) {
  GURL url(ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                     FilePath(kTitle1File)));
  ui_test_utils::NavigateToURL(browser(), url);
  AddTabAtIndex(0, url, content::PAGE_TRANSITION_TYPED);
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(0, browser()->active_index());
  TabContents* second_tab = browser()->GetTabContentsAt(1);
  ASSERT_TRUE(second_tab);
  second_tab->render_view_host()->ExecuteJavascriptInWebFrame(
      string16(),
      ASCIIToUTF16("alert('Activate!');"));
  AppModalDialog* alert = ui_test_utils::WaitForAppModalDialog();
  alert->CloseModalDialog();
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1, browser()->active_index());
}



#if defined(OS_WIN)
// http://crbug.com/75274. On XP crashes inside
// URLFetcher::Core::Registry::RemoveURLFetcherCore.
#define MAYBE_ThirtyFourTabs FLAKY_ThirtyFourTabs
#else
#define MAYBE_ThirtyFourTabs ThirtyFourTabs
#endif

// Create 34 tabs and verify that a lot of processes have been created. The
// exact number of processes depends on the amount of memory. Previously we
// had a hard limit of 31 processes and this test is mainly directed at
// verifying that we don't crash when we pass this limit.
// Warning: this test can take >30 seconds when running on a slow (low
// memory?) Mac builder.
IN_PROC_BROWSER_TEST_F(BrowserTest, MAYBE_ThirtyFourTabs) {
  GURL url(ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                     FilePath(kTitle2File)));

  // There is one initial tab.
  for (int ix = 0; ix != 33; ++ix)
    browser()->AddSelectedTabWithURL(url, content::PAGE_TRANSITION_TYPED);
  EXPECT_EQ(34, browser()->tab_count());

  // See browser\renderer_host\render_process_host.cc for the algorithm to
  // decide how many processes to create.
  if (base::SysInfo::AmountOfPhysicalMemoryMB() >= 2048) {
    EXPECT_GE(CountRenderProcessHosts(), 24);
  } else {
    EXPECT_LE(CountRenderProcessHosts(), 23);
  }
}

// Test for crbug.com/22004.  Reloading a page with a before unload handler and
// then canceling the dialog should not leave the throbber spinning.
IN_PROC_BROWSER_TEST_F(BrowserTest, ReloadThenCancelBeforeUnload) {
  GURL url(std::string("data:text/html,") + kBeforeUnloadHTML);
  ui_test_utils::NavigateToURL(browser(), url);

  // Navigate to another page, but click cancel in the dialog.  Make sure that
  // the throbber stops spinning.
  browser()->Reload(CURRENT_TAB);
  AppModalDialog* alert = ui_test_utils::WaitForAppModalDialog();
  alert->CloseModalDialog();
  EXPECT_FALSE(browser()->GetSelectedTabContents()->IsLoading());

  // Clear the beforeunload handler so the test can easily exit.
  browser()->GetSelectedTabContents()->render_view_host()->
      ExecuteJavascriptInWebFrame(string16(),
                                  ASCIIToUTF16("onbeforeunload=null;"));
}

// Test for crbug.com/80401.  Canceling a before unload dialog should reset
// the URL to the previous page's URL.
IN_PROC_BROWSER_TEST_F(BrowserTest, CancelBeforeUnloadResetsURL) {
  GURL url(ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                     FilePath(kBeforeUnloadFile)));
  ui_test_utils::NavigateToURL(browser(), url);

  // Navigate to a page that triggers a cross-site transition.
  ASSERT_TRUE(test_server()->Start());
  GURL url2(test_server()->GetURL("files/title1.html"));
  browser()->OpenURL(url2, GURL(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED);

  ui_test_utils::WindowedNotificationObserver host_destroyed_observer(
      content::NOTIFICATION_RENDER_WIDGET_HOST_DESTROYED,
      content::NotificationService::AllSources());

  // Cancel the dialog.
  AppModalDialog* alert = ui_test_utils::WaitForAppModalDialog();
  alert->CloseModalDialog();
  EXPECT_FALSE(browser()->GetSelectedTabContents()->IsLoading());

  // Wait for the ShouldClose_ACK to arrive.  We can detect it by waiting for
  // the pending RVH to be destroyed.
  host_destroyed_observer.Wait();
  EXPECT_EQ(url.spec(), UTF16ToUTF8(browser()->toolbar_model()->GetText()));

  // Clear the beforeunload handler so the test can easily exit.
  browser()->GetSelectedTabContents()->render_view_host()->
      ExecuteJavascriptInWebFrame(string16(),
                                  ASCIIToUTF16("onbeforeunload=null;"));
}

// Crashy on mac.  http://crbug.com/38522
#if defined(OS_MACOSX)
#define MAYBE_SingleBeforeUnloadAfterWindowClose \
        DISABLED_SingleBeforeUnloadAfterWindowClose
#else
#define MAYBE_SingleBeforeUnloadAfterWindowClose \
        SingleBeforeUnloadAfterWindowClose
#endif

// Test for crbug.com/11647.  A page closed with window.close() should not have
// two beforeunload dialogs shown.
IN_PROC_BROWSER_TEST_F(BrowserTest, MAYBE_SingleBeforeUnloadAfterWindowClose) {
  browser()->GetSelectedTabContents()->render_view_host()->
      ExecuteJavascriptInWebFrame(string16(),
                                  ASCIIToUTF16(kOpenNewBeforeUnloadPage));

  // Close the new window with JavaScript, which should show a single
  // beforeunload dialog.  Then show another alert, to make it easy to verify
  // that a second beforeunload dialog isn't shown.
  browser()->GetTabContentsAt(0)->render_view_host()->
      ExecuteJavascriptInWebFrame(string16(),
                                  ASCIIToUTF16("w.close(); alert('bar');"));
  AppModalDialog* alert = ui_test_utils::WaitForAppModalDialog();
  alert->native_dialog()->AcceptAppModalDialog();

  alert = ui_test_utils::WaitForAppModalDialog();
  EXPECT_FALSE(static_cast<JavaScriptAppModalDialog*>(alert)->
                   is_before_unload_dialog());
  alert->native_dialog()->AcceptAppModalDialog();
}

// Test that scripts can fork a new renderer process for a cross-site popup,
// based on http://www.google.com/chrome/intl/en/webmasters-faq.html#newtab.
// The script must open a new tab, set its window.opener to null, and navigate
// it to a cross-site URL.  It should also work for meta-refreshes.
// See http://crbug.com/93517.
IN_PROC_BROWSER_TEST_F(BrowserTest, NullOpenerRedirectForksProcess) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);

  // Create http and https servers for a cross-site transition.
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_test_server(net::TestServer::TYPE_HTTPS,
                                    FilePath(kDocRoot));
  ASSERT_TRUE(https_test_server.Start());
  GURL http_url(test_server()->GetURL("files/title1.html"));
  GURL https_url(https_test_server.GetURL(""));

  // Start with an http URL.
  ui_test_utils::NavigateToURL(browser(), http_url);
  TabContents* oldtab = browser()->GetSelectedTabContents();
  content::RenderProcessHost* process = oldtab->render_view_host()->process();

  // Now open a tab to a blank page, set its opener to null, and redirect it
  // cross-site.
  std::string redirect_popup = "w=window.open();";
  redirect_popup += "w.opener=null;";
  redirect_popup += "w.document.location=\"";
  redirect_popup += https_url.spec();
  redirect_popup += "\";";

  ui_test_utils::WindowedNotificationObserver popup_observer(
        content::NOTIFICATION_TAB_ADDED,
        content::NotificationService::AllSources());
  ui_test_utils::WindowedNotificationObserver nav_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::NotificationService::AllSources());
  oldtab->render_view_host()->
      ExecuteJavascriptInWebFrame(string16(), ASCIIToUTF16(redirect_popup));

  // Wait for popup window to appear and finish navigating.
  popup_observer.Wait();
  ASSERT_EQ(2, browser()->tab_count());
  TabContents* newtab = browser()->GetSelectedTabContents();
  EXPECT_TRUE(newtab);
  EXPECT_NE(oldtab, newtab);
  nav_observer.Wait();
  ASSERT_TRUE(newtab->controller().GetLastCommittedEntry());
  EXPECT_EQ(https_url.spec(),
            newtab->controller().GetLastCommittedEntry()->url().spec());

  // Popup window should not be in the opener's process.
  content::RenderProcessHost* popup_process =
      newtab->render_view_host()->process();
  EXPECT_NE(process, popup_process);

  // Now open a tab to a blank page, set its opener to null, and use a
  // meta-refresh to navigate it instead.
  std::string refresh_popup = "w=window.open();";
  refresh_popup += "w.opener=null;";
  refresh_popup += "w.document.write(";
  refresh_popup += "'<META HTTP-EQUIV=\"refresh\" content=\"0; url=";
  refresh_popup += https_url.spec();
  refresh_popup += "\">');w.document.close();";

  ui_test_utils::WindowedNotificationObserver popup_observer2(
        content::NOTIFICATION_TAB_ADDED,
        content::NotificationService::AllSources());
  ui_test_utils::WindowedNotificationObserver nav_observer2(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::NotificationService::AllSources());
  oldtab->render_view_host()->
      ExecuteJavascriptInWebFrame(string16(), ASCIIToUTF16(refresh_popup));

  // Wait for popup window to appear and finish navigating.
  popup_observer2.Wait();
  ASSERT_EQ(3, browser()->tab_count());
  TabContents* newtab2 = browser()->GetSelectedTabContents();
  EXPECT_TRUE(newtab2);
  EXPECT_NE(oldtab, newtab2);
  nav_observer2.Wait();
  ASSERT_TRUE(newtab2->controller().GetLastCommittedEntry());
  EXPECT_EQ(https_url.spec(),
            newtab2->controller().GetLastCommittedEntry()->url().spec());

  // This popup window should also not be in the opener's process.
  content::RenderProcessHost* popup_process2 =
      newtab2->render_view_host()->process();
  EXPECT_NE(process, popup_process2);
}

// Tests that other popup navigations that do not follow the steps at
// http://www.google.com/chrome/intl/en/webmasters-faq.html#newtab will not
// fork a new renderer process.
IN_PROC_BROWSER_TEST_F(BrowserTest, OtherRedirectsDontForkProcess) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisablePopupBlocking);

  // Create http and https servers for a cross-site transition.
  ASSERT_TRUE(test_server()->Start());
  net::TestServer https_test_server(net::TestServer::TYPE_HTTPS,
                                    FilePath(kDocRoot));
  ASSERT_TRUE(https_test_server.Start());
  GURL http_url(test_server()->GetURL("files/title1.html"));
  GURL https_url(https_test_server.GetURL(""));

  // Start with an http URL.
  ui_test_utils::NavigateToURL(browser(), http_url);
  TabContents* oldtab = browser()->GetSelectedTabContents();
  content::RenderProcessHost* process = oldtab->render_view_host()->process();

  // Now open a tab to a blank page, set its opener to null, and redirect it
  // cross-site.
  std::string dont_fork_popup = "w=window.open();";
  dont_fork_popup += "w.document.location=\"";
  dont_fork_popup += https_url.spec();
  dont_fork_popup += "\";";

  ui_test_utils::WindowedNotificationObserver popup_observer(
        content::NOTIFICATION_TAB_ADDED,
        content::NotificationService::AllSources());
  ui_test_utils::WindowedNotificationObserver nav_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::NotificationService::AllSources());
  oldtab->render_view_host()->
      ExecuteJavascriptInWebFrame(string16(), ASCIIToUTF16(dont_fork_popup));

  // Wait for popup window to appear and finish navigating.
  popup_observer.Wait();
  ASSERT_EQ(2, browser()->tab_count());
  TabContents* newtab = browser()->GetSelectedTabContents();
  EXPECT_TRUE(newtab);
  EXPECT_NE(oldtab, newtab);
  nav_observer.Wait();
  ASSERT_TRUE(newtab->controller().GetLastCommittedEntry());
  EXPECT_EQ(https_url.spec(),
            newtab->controller().GetLastCommittedEntry()->url().spec());

  // Popup window should still be in the opener's process.
  content::RenderProcessHost* popup_process =
      newtab->render_view_host()->process();
  EXPECT_EQ(process, popup_process);

  // Same thing if the current tab tries to navigate itself.
  std::string navigate_str = "document.location=\"";
  navigate_str += https_url.spec();
  navigate_str += "\";";

  ui_test_utils::WindowedNotificationObserver nav_observer2(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::NotificationService::AllSources());
  oldtab->render_view_host()->
      ExecuteJavascriptInWebFrame(string16(), ASCIIToUTF16(navigate_str));
  nav_observer2.Wait();
  ASSERT_TRUE(oldtab->controller().GetLastCommittedEntry());
  EXPECT_EQ(https_url.spec(),
            oldtab->controller().GetLastCommittedEntry()->url().spec());

  // Original window should still be in the original process.
  content::RenderProcessHost* new_process =
      newtab->render_view_host()->process();
  EXPECT_EQ(process, new_process);
}

// Test that get_process_idle_time() returns reasonable values when compared
// with time deltas measured locally.
IN_PROC_BROWSER_TEST_F(BrowserTest, RenderIdleTime) {
  base::TimeTicks start = base::TimeTicks::Now();
  ui_test_utils::NavigateToURL(browser(),
      ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                FilePath(kTitle1File)));
  content::RenderProcessHost::iterator it(
      content::RenderProcessHost::AllHostsIterator());
  for (; !it.IsAtEnd(); it.Advance()) {
    base::TimeDelta renderer_td =
        it.GetCurrentValue()->GetChildProcessIdleTime();
    base::TimeDelta browser_td = base::TimeTicks::Now() - start;
    EXPECT_TRUE(browser_td >= renderer_td);
  }
}

// Test IDC_CREATE_SHORTCUTS command is enabled for url scheme file, ftp, http
// and https and disabled for chrome://, about:// etc.
// TODO(pinkerton): Disable app-mode in the model until we implement it
// on the Mac. http://crbug.com/13148
#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(BrowserTest, CommandCreateAppShortcutFile) {
  CommandUpdater* command_updater = browser()->command_updater();

  static const FilePath::CharType* kEmptyFile = FILE_PATH_LITERAL("empty.html");
  GURL file_url(ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                          FilePath(kEmptyFile)));
  ASSERT_TRUE(file_url.SchemeIs(chrome::kFileScheme));
  ui_test_utils::NavigateToURL(browser(), file_url);
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_CREATE_SHORTCUTS));
}

IN_PROC_BROWSER_TEST_F(BrowserTest, CommandCreateAppShortcutHttp) {
  CommandUpdater* command_updater = browser()->command_updater();

  ASSERT_TRUE(test_server()->Start());
  GURL http_url(test_server()->GetURL(""));
  ASSERT_TRUE(http_url.SchemeIs(chrome::kHttpScheme));
  ui_test_utils::NavigateToURL(browser(), http_url);
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_CREATE_SHORTCUTS));
}

IN_PROC_BROWSER_TEST_F(BrowserTest, CommandCreateAppShortcutHttps) {
  CommandUpdater* command_updater = browser()->command_updater();

  net::TestServer test_server(net::TestServer::TYPE_HTTPS, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());
  GURL https_url(test_server.GetURL("/"));
  ASSERT_TRUE(https_url.SchemeIs(chrome::kHttpsScheme));
  ui_test_utils::NavigateToURL(browser(), https_url);
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_CREATE_SHORTCUTS));
}

IN_PROC_BROWSER_TEST_F(BrowserTest, CommandCreateAppShortcutFtp) {
  CommandUpdater* command_updater = browser()->command_updater();

  net::TestServer test_server(net::TestServer::TYPE_FTP, FilePath(kDocRoot));
  ASSERT_TRUE(test_server.Start());
  GURL ftp_url(test_server.GetURL(""));
  ASSERT_TRUE(ftp_url.SchemeIs(chrome::kFtpScheme));
  ui_test_utils::NavigateToURL(browser(), ftp_url);
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_CREATE_SHORTCUTS));
}

IN_PROC_BROWSER_TEST_F(BrowserTest, CommandCreateAppShortcutInvalid) {
  CommandUpdater* command_updater = browser()->command_updater();

  // Urls that should not have shortcuts.
  GURL new_tab_url(chrome::kChromeUINewTabURL);
  ui_test_utils::NavigateToURL(browser(), new_tab_url);
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_CREATE_SHORTCUTS));

  GURL history_url(chrome::kChromeUIHistoryURL);
  ui_test_utils::NavigateToURL(browser(), history_url);
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_CREATE_SHORTCUTS));

  GURL downloads_url(chrome::kChromeUIDownloadsURL);
  ui_test_utils::NavigateToURL(browser(), downloads_url);
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_CREATE_SHORTCUTS));

  GURL blank_url(chrome::kAboutBlankURL);
  ui_test_utils::NavigateToURL(browser(), blank_url);
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_CREATE_SHORTCUTS));
}

// Change a tab into an application window.
// DISABLED: http://crbug.com/72310
IN_PROC_BROWSER_TEST_F(BrowserTest, DISABLED_ConvertTabToAppShortcut) {
  ASSERT_TRUE(test_server()->Start());
  GURL http_url(test_server()->GetURL(""));
  ASSERT_TRUE(http_url.SchemeIs(chrome::kHttpScheme));

  ASSERT_EQ(1, browser()->tab_count());
  TabContents* initial_tab = browser()->GetTabContentsAt(0);
  TabContents* app_tab = browser()->AddSelectedTabWithURL(
      http_url, content::PAGE_TRANSITION_TYPED)->tab_contents();
  ASSERT_EQ(2, browser()->tab_count());
  ASSERT_EQ(1u, BrowserList::GetBrowserCount(browser()->profile()));

  // Normal tabs should accept load drops.
  EXPECT_TRUE(initial_tab->GetMutableRendererPrefs()->can_accept_load_drops);
  EXPECT_TRUE(app_tab->GetMutableRendererPrefs()->can_accept_load_drops);

  // Turn |app_tab| into a tab in an app panel.
  browser()->ConvertContentsToApplication(app_tab);

  // The launch should have created a new browser.
  ASSERT_EQ(2u, BrowserList::GetBrowserCount(browser()->profile()));

  // Find the new browser.
  Browser* app_browser = NULL;
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end() && !app_browser; ++i) {
    if (*i != browser())
      app_browser = *i;
  }
  ASSERT_TRUE(app_browser);

  // Check that the tab contents is in the new browser, and not in the old.
  ASSERT_EQ(1, browser()->tab_count());
  ASSERT_EQ(initial_tab, browser()->GetTabContentsAt(0));

  // Check that the appliaction browser has a single tab, and that tab contains
  // the content that we app-ified.
  ASSERT_EQ(1, app_browser->tab_count());
  ASSERT_EQ(app_tab, app_browser->GetTabContentsAt(0));

  // Normal tabs should accept load drops.
  EXPECT_TRUE(initial_tab->GetMutableRendererPrefs()->can_accept_load_drops);

  // The tab in an app window should not.
  EXPECT_FALSE(app_tab->GetMutableRendererPrefs()->can_accept_load_drops);
}

#endif  // !defined(OS_MACOSX)

// Test RenderView correctly send back favicon url for web page that redirects
// to an anchor in javascript body.onload handler.
IN_PROC_BROWSER_TEST_F(BrowserTest,
                       DISABLED_FaviconOfOnloadRedirectToAnchorPage) {
  ASSERT_TRUE(test_server()->Start());
  GURL url(test_server()->GetURL("files/onload_redirect_to_anchor.html"));
  GURL expected_favicon_url(test_server()->GetURL("files/test.png"));

  ui_test_utils::NavigateToURL(browser(), url);

  NavigationEntry* entry = browser()->GetSelectedTabContents()->
      controller().GetActiveEntry();
  EXPECT_EQ(expected_favicon_url.spec(), entry->favicon().url().spec());
}

#if defined(OS_MACOSX) || defined(OS_LINUX)
// http://crbug.com/83828. On Mac 10.6, the failure rate is 14%
#define MAYBE_FaviconChange FLAKY_FaviconChange
#else
#define MAYBE_FaviconChange FaviconChange
#endif
// Test that an icon can be changed from JS.
IN_PROC_BROWSER_TEST_F(BrowserTest, MAYBE_FaviconChange) {
  static const FilePath::CharType* kFile =
      FILE_PATH_LITERAL("onload_change_favicon.html");
  GURL file_url(ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                          FilePath(kFile)));
  ASSERT_TRUE(file_url.SchemeIs(chrome::kFileScheme));
  ui_test_utils::NavigateToURL(browser(), file_url);

  NavigationEntry* entry = browser()->GetSelectedTabContents()->
      controller().GetActiveEntry();
  static const FilePath::CharType* kIcon =
      FILE_PATH_LITERAL("test1.png");
  GURL expected_favicon_url(
      ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                         FilePath(kIcon)));
  EXPECT_EQ(expected_favicon_url.spec(), entry->favicon().url().spec());
}

// Makes sure TabClosing is sent when uninstalling an extension that is an app
// tab.
IN_PROC_BROWSER_TEST_F(BrowserTest, TabClosingWhenRemovingExtension) {
  ASSERT_TRUE(test_server()->Start());
  host_resolver()->AddRule("www.example.com", "127.0.0.1");
  GURL url(test_server()->GetURL("empty.html"));
  TabStripModel* model = browser()->tabstrip_model();

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app/")));

  const Extension* extension_app = GetExtension();

  ui_test_utils::NavigateToURL(browser(), url);

  TabContentsWrapper* app_contents =
      Browser::TabContentsFactory(browser()->profile(), NULL,
                                  MSG_ROUTING_NONE, NULL, NULL);
  app_contents->extension_tab_helper()->SetExtensionApp(extension_app);

  model->AddTabContents(app_contents, 0, content::PageTransitionFromInt(0),
                        TabStripModel::ADD_NONE);
  model->SetTabPinned(0, true);
  ui_test_utils::NavigateToURL(browser(), url);

  MockTabStripModelObserver observer;
  model->AddObserver(&observer);

  // Uninstall the extension and make sure TabClosing is sent.
  ExtensionService* service = browser()->profile()->GetExtensionService();
  service->UninstallExtension(GetExtension()->id(), false, NULL);
  EXPECT_EQ(1, observer.closing_count());

  model->RemoveObserver(&observer);

  // There should only be one tab now.
  ASSERT_EQ(1, browser()->tab_count());
}

#if !defined(OS_MACOSX)
// Open with --app-id=<id>, and see that an app window opens.
IN_PROC_BROWSER_TEST_F(BrowserTest, AppIdSwitch) {
  ASSERT_TRUE(test_server()->Start());

  // Load an app.
  host_resolver()->AddRule("www.example.com", "127.0.0.1");
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app/")));
  const Extension* extension_app = GetExtension();

  CommandLine command_line(CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, extension_app->id());

  BrowserInit::IsFirstRun first_run = FirstRun::IsChromeFirstRun() ?
      BrowserInit::IS_FIRST_RUN : BrowserInit::IS_NOT_FIRST_RUN;
  BrowserInit::LaunchWithProfile launch(FilePath(), command_line, first_run);
  ASSERT_TRUE(launch.OpenApplicationWindow(browser()->profile()));

  // Check that the new browser has an app name.
  // The launch should have created a new browser.
  ASSERT_EQ(2u, BrowserList::GetBrowserCount(browser()->profile()));

  // Find the new browser.
  Browser* new_browser = NULL;
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end() && !new_browser; ++i) {
    if (*i != browser())
      new_browser = *i;
  }
  ASSERT_TRUE(new_browser);
  ASSERT_TRUE(new_browser != browser());

  // The browser's app_name should include the app's ID.
  ASSERT_NE(
      new_browser->app_name_.find(extension_app->id()),
      std::string::npos) << new_browser->app_name_;
}
#endif

#if defined(OS_WIN) || defined(OS_MACOSX)
// http://crbug.com/46198: On XP/Vista, the failure rate is 5 ~ 6%.
// On OS 10.5 it's 1%, 10.6 currently 4%.
#define MAYBE_PageLanguageDetection FLAKY_PageLanguageDetection
#else
#define MAYBE_PageLanguageDetection PageLanguageDetection
#endif
// Tests that the CLD (Compact Language Detection) works properly.
IN_PROC_BROWSER_TEST_F(BrowserTest, MAYBE_PageLanguageDetection) {
  ASSERT_TRUE(test_server()->Start());

  std::string lang;

  // Open a new tab with a page in English.
  AddTabAtIndex(0, GURL(test_server()->GetURL("files/english_page.html")),
                content::PAGE_TRANSITION_TYPED);

  TabContents* current_tab = browser()->GetSelectedTabContents();
  TabContentsWrapper* wrapper = browser()->GetSelectedTabContentsWrapper();
  TranslateTabHelper* helper = wrapper->translate_tab_helper();
  content::Source<TabContents> source(current_tab);

  ui_test_utils::WindowedNotificationObserverWithDetails<std::string>
      en_language_detected_signal(chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
                                  source);
  EXPECT_EQ("", helper->language_state().original_language());
  en_language_detected_signal.Wait();
  EXPECT_TRUE(en_language_detected_signal.GetDetailsFor(
        source.map_key(), &lang));
  EXPECT_EQ("en", lang);
  EXPECT_EQ("en", helper->language_state().original_language());

  // Now navigate to a page in French.
  ui_test_utils::WindowedNotificationObserverWithDetails<std::string>
      fr_language_detected_signal(chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
                                  source);
  ui_test_utils::NavigateToURL(
      browser(), GURL(test_server()->GetURL("files/french_page.html")));
  fr_language_detected_signal.Wait();
  lang.clear();
  EXPECT_TRUE(fr_language_detected_signal.GetDetailsFor(
        source.map_key(), &lang));
  EXPECT_EQ("fr", lang);
  EXPECT_EQ("fr", helper->language_state().original_language());
}

#if defined(OS_MACOSX)
// http://crbug.com/104265
#define MAYBE_TestNewTabExitsFullscreen DISABLED_TestNewTabExitsFullscreen
#else
#define MAYBE_TestNewTabExitsFullscreen TestNewTabExitsFullscreen
#endif

IN_PROC_BROWSER_TEST_F(BrowserTest, MAYBE_TestNewTabExitsFullscreen) {
  ASSERT_TRUE(test_server()->Start());

  AddTabAtIndex(
      0, GURL(chrome::kAboutBlankURL), content::PAGE_TRANSITION_TYPED);

  TabContents* fullscreen_tab = browser()->GetSelectedTabContents();

  {
    ui_test_utils::WindowedNotificationObserver fullscreen_observer(
        chrome::NOTIFICATION_FULLSCREEN_CHANGED,
        content::NotificationService::AllSources());
    browser()->ToggleFullscreenModeForTab(fullscreen_tab, true);
    fullscreen_observer.Wait();
    ASSERT_TRUE(browser()->window()->IsFullscreen());
  }

  {
    ui_test_utils::WindowedNotificationObserver fullscreen_observer(
        chrome::NOTIFICATION_FULLSCREEN_CHANGED,
        content::NotificationService::AllSources());
    AddTabAtIndex(
        1, GURL(chrome::kAboutBlankURL), content::PAGE_TRANSITION_TYPED);
    fullscreen_observer.Wait();
    ASSERT_FALSE(browser()->window()->IsFullscreen());
  }
}

#if defined(OS_MACOSX)
// http://crbug.com/100467
#define MAYBE_TestTabExitsItselfFromFullscreen \
        FAILS_TestTabExitsItselfFromFullscreen
#else
#define MAYBE_TestTabExitsItselfFromFullscreen TestTabExitsItselfFromFullscreen
#endif

IN_PROC_BROWSER_TEST_F(BrowserTest, MAYBE_TestTabExitsItselfFromFullscreen) {
  ASSERT_TRUE(test_server()->Start());

  AddTabAtIndex(
      0, GURL(chrome::kAboutBlankURL), content::PAGE_TRANSITION_TYPED);

  TabContents* fullscreen_tab = browser()->GetSelectedTabContents();

  {
    ui_test_utils::WindowedNotificationObserver fullscreen_observer(
        chrome::NOTIFICATION_FULLSCREEN_CHANGED,
        content::NotificationService::AllSources());
    browser()->ToggleFullscreenModeForTab(fullscreen_tab, true);
    fullscreen_observer.Wait();
    ASSERT_TRUE(browser()->window()->IsFullscreen());
  }

  {
    ui_test_utils::WindowedNotificationObserver fullscreen_observer(
        chrome::NOTIFICATION_FULLSCREEN_CHANGED,
        content::NotificationService::AllSources());
    browser()->ToggleFullscreenModeForTab(fullscreen_tab, false);
    fullscreen_observer.Wait();
    ASSERT_FALSE(browser()->window()->IsFullscreen());
  }
}

IN_PROC_BROWSER_TEST_F(BrowserTest, TestFullscreenBubbleMouseLockState) {
  ASSERT_TRUE(test_server()->Start());

  AddTabAtIndex(0, GURL(chrome::kAboutBlankURL),
                content::PAGE_TRANSITION_TYPED);
  AddTabAtIndex(1, GURL(chrome::kAboutBlankURL),
                content::PAGE_TRANSITION_TYPED);

  TabContents* fullscreen_tab = browser()->GetSelectedTabContents();

  {
    ui_test_utils::WindowedNotificationObserver fullscreen_observer(
        chrome::NOTIFICATION_FULLSCREEN_CHANGED,
        content::NotificationService::AllSources());
    browser()->ToggleFullscreenModeForTab(fullscreen_tab, true);
    fullscreen_observer.Wait();
    ASSERT_TRUE(browser()->window()->IsFullscreen());
  }

  browser()->RequestToLockMouse(fullscreen_tab);
  FullscreenExitBubbleType type = browser()->GetFullscreenExitBubbleType();
  bool mouse_lock = false;
  fullscreen_bubble::PermissionRequestedByType(type, NULL, &mouse_lock);
  ASSERT_TRUE(mouse_lock);

  browser()->OnAcceptFullscreenPermission(fullscreen_tab->GetURL(), type);
  type = browser()->GetFullscreenExitBubbleType();
  ASSERT_FALSE(fullscreen_bubble::ShowButtonsForType(type));
}

#if defined(OS_MACOSX)
// http://crbug.com/100467
IN_PROC_BROWSER_TEST_F(
    BrowserTest, FAILS_TabEntersPresentationModeFromWindowed) {
  ASSERT_TRUE(test_server()->Start());

  AddTabAtIndex(
      0, GURL(chrome::kAboutBlankURL), content::PAGE_TRANSITION_TYPED);

  TabContents* fullscreen_tab = browser()->GetSelectedTabContents();

  {
    ui_test_utils::WindowedNotificationObserver fullscreen_observer(
        chrome::NOTIFICATION_FULLSCREEN_CHANGED,
        content::NotificationService::AllSources());
    EXPECT_FALSE(browser()->window()->IsFullscreen());
    EXPECT_FALSE(browser()->window()->InPresentationMode());
    browser()->ToggleFullscreenModeForTab(fullscreen_tab, true);
    fullscreen_observer.Wait();
    ASSERT_TRUE(browser()->window()->IsFullscreen());
    ASSERT_TRUE(browser()->window()->InPresentationMode());
  }

  {
    ui_test_utils::WindowedNotificationObserver fullscreen_observer(
        chrome::NOTIFICATION_FULLSCREEN_CHANGED,
        content::NotificationService::AllSources());
    browser()->TogglePresentationMode(false);
    fullscreen_observer.Wait();
    ASSERT_FALSE(browser()->window()->IsFullscreen());
    ASSERT_FALSE(browser()->window()->InPresentationMode());
  }

  if (base::mac::IsOSLionOrLater()) {
    // Test that tab fullscreen mode doesn't make presentation mode the default
    // on Lion.
    ui_test_utils::WindowedNotificationObserver fullscreen_observer(
        chrome::NOTIFICATION_FULLSCREEN_CHANGED,
        content::NotificationService::AllSources());
    browser()->ToggleFullscreenMode(false);
    fullscreen_observer.Wait();
    ASSERT_TRUE(browser()->window()->IsFullscreen());
    ASSERT_FALSE(browser()->window()->InPresentationMode());
  }
}
#endif

// Chromeos defaults to restoring the last session, so this test isn't
// applicable.
#if !defined(OS_CHROMEOS)
#if defined(OS_MACOSX)
// Crashy, http://crbug.com/38522
#define RestorePinnedTabs DISABLED_RestorePinnedTabs
#endif
// Makes sure pinned tabs are restored correctly on start.
IN_PROC_BROWSER_TEST_F(BrowserTest, RestorePinnedTabs) {
  ASSERT_TRUE(test_server()->Start());

  // Add an pinned app tab.
  host_resolver()->AddRule("www.example.com", "127.0.0.1");
  GURL url(test_server()->GetURL("empty.html"));
  TabStripModel* model = browser()->tabstrip_model();
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app/")));
  const Extension* extension_app = GetExtension();
  ui_test_utils::NavigateToURL(browser(), url);
  TabContentsWrapper* app_contents =
    Browser::TabContentsFactory(browser()->profile(), NULL,
                                MSG_ROUTING_NONE, NULL, NULL);
  app_contents->extension_tab_helper()->SetExtensionApp(extension_app);
  model->AddTabContents(app_contents, 0, content::PageTransitionFromInt(0),
                        TabStripModel::ADD_NONE);
  model->SetTabPinned(0, true);
  ui_test_utils::NavigateToURL(browser(), url);

  // Add a non pinned tab.
  browser()->NewTab();

  // Add a pinned non-app tab.
  browser()->NewTab();
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kAboutBlankURL));
  model->SetTabPinned(2, true);

  // Write out the pinned tabs.
  PinnedTabCodec::WritePinnedTabs(browser()->profile());

  // Simulate launching again.
  CommandLine dummy(CommandLine::NO_PROGRAM);
  BrowserInit::IsFirstRun first_run = FirstRun::IsChromeFirstRun() ?
      BrowserInit::IS_FIRST_RUN : BrowserInit::IS_NOT_FIRST_RUN;
  BrowserInit::LaunchWithProfile launch(FilePath(), dummy, first_run);
  launch.profile_ = browser()->profile();
  launch.ProcessStartupURLs(std::vector<GURL>());

  // The launch should have created a new browser.
  ASSERT_EQ(2u, BrowserList::GetBrowserCount(browser()->profile()));

  // Find the new browser.
  Browser* new_browser = NULL;
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end() && !new_browser; ++i) {
    if (*i != browser())
      new_browser = *i;
  }
  ASSERT_TRUE(new_browser);
  ASSERT_TRUE(new_browser != browser());

  // We should get back an additional tab for the app, and another for the
  // default home page.
  ASSERT_EQ(3, new_browser->tab_count());

  // Make sure the state matches.
  TabStripModel* new_model = new_browser->tabstrip_model();
  EXPECT_TRUE(new_model->IsAppTab(0));
  EXPECT_FALSE(new_model->IsAppTab(1));
  EXPECT_FALSE(new_model->IsAppTab(2));

  EXPECT_TRUE(new_model->IsTabPinned(0));
  EXPECT_TRUE(new_model->IsTabPinned(1));
  EXPECT_FALSE(new_model->IsTabPinned(2));

  EXPECT_EQ(browser()->profile()->GetHomePage(),
      new_model->GetTabContentsAt(2)->tab_contents()->GetURL());

  EXPECT_TRUE(
      new_model->GetTabContentsAt(0)->extension_tab_helper()->extension_app() ==
          extension_app);
}
#endif  // !defined(OS_CHROMEOS)

// This test verifies we don't crash when closing the last window and the app
// menu is showing.
IN_PROC_BROWSER_TEST_F(BrowserTest, CloseWithAppMenuOpen) {
  if (browser_defaults::kBrowserAliveWithNoWindows)
    return;

  // We need a message loop running for menus on windows.
  MessageLoop::current()->PostTask(FROM_HERE,
                                   new RunCloseWithAppMenuTask(browser()));
}

#if !defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(BrowserTest, OpenAppWindowLikeNtp) {
  ASSERT_TRUE(test_server()->Start());

  // Load an app
  host_resolver()->AddRule("www.example.com", "127.0.0.1");
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("app/")));
  const Extension* extension_app = GetExtension();

  // Launch it in a window, as AppLauncherHandler::HandleLaunchApp() would.
  TabContents* app_window =
      Browser::OpenApplication(browser()->profile(),
                               extension_app,
                               extension_misc::LAUNCH_WINDOW,
                               GURL(),
                               NEW_WINDOW);
  ASSERT_TRUE(app_window);

  // Apps launched in a window from the NTP do not have extension_app set in
  // tab contents.
  TabContentsWrapper* wrapper =
          TabContentsWrapper::GetCurrentWrapperForContents(app_window);
  EXPECT_FALSE(wrapper->extension_tab_helper()->extension_app());
  EXPECT_EQ(extension_app->GetFullLaunchURL(), app_window->GetURL());

  // The launch should have created a new browser.
  ASSERT_EQ(2u, BrowserList::GetBrowserCount(browser()->profile()));

  // Find the new browser.
  Browser* new_browser = NULL;
  for (BrowserList::const_iterator i = BrowserList::begin();
       i != BrowserList::end() && !new_browser; ++i) {
    if (*i != browser())
      new_browser = *i;
  }
  ASSERT_TRUE(new_browser);
  ASSERT_TRUE(new_browser != browser());

  EXPECT_TRUE(new_browser->is_app());

  // The browser's app name should include the extension's id.
  std::string app_name = new_browser->app_name_;
  EXPECT_NE(app_name.find(extension_app->id()), std::string::npos)
      << "Name " << app_name << " should contain id "<< extension_app->id();
}
#endif  // !defined(OS_MACOSX)

// Makes sure the browser doesn't crash when
// set_show_state(ui::SHOW_STATE_MAXIMIZED) has been invoked.
IN_PROC_BROWSER_TEST_F(BrowserTest, StartMaximized) {
  // Can't test TYPE_PANEL as they are currently created differently (and can't
  // end up maximized).
  Browser::Type types[] = { Browser::TYPE_TABBED, Browser::TYPE_POPUP };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(types); ++i) {
    Browser* max_browser = new Browser(types[i], browser()->profile());
    max_browser->set_show_state(ui::SHOW_STATE_MAXIMIZED);
    max_browser->InitBrowserWindow();
    AddBlankTabAndShow(max_browser);
  }
}

// Aura doesn't support minimized window. crbug.com/104571.
#if defined(USE_AURA)
#define MAYBE_StartMinimized DISABLED_StartMinimized
#else
#define MAYBE_StartMinimized StartMinimized
#endif
// Makes sure the browser doesn't crash when
// set_show_state(ui::SHOW_STATE_MINIMIZED) has been invoked.
IN_PROC_BROWSER_TEST_F(BrowserTest, MAYBE_StartMinimized) {
  // Can't test TYPE_PANEL as they are currently created differently (and can't
  // end up minimized).
  Browser::Type types[] = { Browser::TYPE_TABBED, Browser::TYPE_POPUP };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(types); ++i) {
    Browser* min_browser = new Browser(types[i], browser()->profile());
    min_browser->set_show_state(ui::SHOW_STATE_MINIMIZED);
    min_browser->InitBrowserWindow();
    AddBlankTabAndShow(min_browser);
  }
}

// Makes sure the forward button is disabled immediately when navigating
// forward to a slow-to-commit page.
IN_PROC_BROWSER_TEST_F(BrowserTest, ForwardDisabledOnForward) {
  GURL blank_url(chrome::kAboutBlankURL);
  ui_test_utils::NavigateToURL(browser(), blank_url);

  ui_test_utils::NavigateToURL(browser(),
      ui_test_utils::GetTestUrl(FilePath(FilePath::kCurrentDirectory),
                                FilePath(kTitle1File)));

  ui_test_utils::WindowedNotificationObserver back_nav_load_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &browser()->GetSelectedTabContents()->controller()));
  browser()->GoBack(CURRENT_TAB);
  back_nav_load_observer.Wait();
  EXPECT_TRUE(browser()->command_updater()->IsCommandEnabled(IDC_FORWARD));

  ui_test_utils::WindowedNotificationObserver forward_nav_load_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &browser()->GetSelectedTabContents()->controller()));
  browser()->GoForward(CURRENT_TAB);
  // This check will happen before the navigation completes, since the browser
  // won't process the renderer's response until the Wait() call below.
  EXPECT_FALSE(browser()->command_updater()->IsCommandEnabled(IDC_FORWARD));
  forward_nav_load_observer.Wait();
}

// Makes sure certain commands are disabled when Incognito mode is forced.
IN_PROC_BROWSER_TEST_F(BrowserTest, DisableMenuItemsWhenIncognitoIsForced) {
  CommandUpdater* command_updater = browser()->command_updater();
  // At the beginning, all commands are enabled.
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_NEW_WINDOW));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_NEW_INCOGNITO_WINDOW));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SHOW_BOOKMARK_MANAGER));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_MANAGE_EXTENSIONS));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_OPTIONS));

  // Set Incognito to FORCED.
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::FORCED);
  // Bookmarks & Settings commands should get disabled.
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_NEW_WINDOW));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_SHOW_BOOKMARK_MANAGER));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_MANAGE_EXTENSIONS));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_OPTIONS));
  // New Incognito Window command, however, should be enabled.
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_NEW_INCOGNITO_WINDOW));

  // Create a new browser.
  Browser* new_browser = Browser::Create(browser()->profile());
  CommandUpdater* new_command_updater = new_browser->command_updater();
  // It should have Bookmarks & Settings commands disabled by default.
  EXPECT_FALSE(new_command_updater->IsCommandEnabled(IDC_NEW_WINDOW));
  EXPECT_FALSE(new_command_updater->IsCommandEnabled(
      IDC_SHOW_BOOKMARK_MANAGER));
  EXPECT_FALSE(new_command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));
  EXPECT_FALSE(new_command_updater->IsCommandEnabled(IDC_MANAGE_EXTENSIONS));
  EXPECT_FALSE(new_command_updater->IsCommandEnabled(IDC_OPTIONS));
  EXPECT_TRUE(new_command_updater->IsCommandEnabled(IDC_NEW_INCOGNITO_WINDOW));
}

// Makes sure New Incognito Window command is disabled when Incognito mode is
// not available.
IN_PROC_BROWSER_TEST_F(BrowserTest,
                       NoNewIncognitoWindowWhenIncognitoIsDisabled) {
  CommandUpdater* command_updater = browser()->command_updater();
  // Set Incognito to DISABLED.
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::DISABLED);
  // Make sure New Incognito Window command is disabled. All remaining commands
  // should be enabled.
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_NEW_INCOGNITO_WINDOW));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_NEW_WINDOW));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SHOW_BOOKMARK_MANAGER));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_MANAGE_EXTENSIONS));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_OPTIONS));

  // Create a new browser.
  Browser* new_browser = Browser::Create(browser()->profile());
  CommandUpdater* new_command_updater = new_browser->command_updater();
  EXPECT_FALSE(new_command_updater->IsCommandEnabled(IDC_NEW_INCOGNITO_WINDOW));
  EXPECT_TRUE(new_command_updater->IsCommandEnabled(IDC_NEW_WINDOW));
  EXPECT_TRUE(new_command_updater->IsCommandEnabled(IDC_SHOW_BOOKMARK_MANAGER));
  EXPECT_TRUE(new_command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));
  EXPECT_TRUE(new_command_updater->IsCommandEnabled(IDC_MANAGE_EXTENSIONS));
  EXPECT_TRUE(new_command_updater->IsCommandEnabled(IDC_OPTIONS));
}

// Makes sure Extensions and Settings commands are disabled in certain
// circumstances even though normally they should stay enabled.
IN_PROC_BROWSER_TEST_F(BrowserTest,
                       DisableExtensionsAndSettingsWhenIncognitoIsDisabled) {
  CommandUpdater* command_updater = browser()->command_updater();
  // Disable extensions. This should disable Extensions menu.
  browser()->profile()->GetExtensionService()->set_extensions_enabled(false);
  // Set Incognito to DISABLED.
  IncognitoModePrefs::SetAvailability(browser()->profile()->GetPrefs(),
                                      IncognitoModePrefs::DISABLED);
  // Make sure Manage Extensions command is disabled.
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_MANAGE_EXTENSIONS));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_NEW_WINDOW));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_SHOW_BOOKMARK_MANAGER));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));
  EXPECT_TRUE(command_updater->IsCommandEnabled(IDC_OPTIONS));

  // Create a popup (non-main-UI-type) browser. Settings command as well
  // as Extensions should be disabled.
  Browser* popup_browser = browser()->CreateForType(Browser::TYPE_POPUP,
                                                    browser()->profile());
  CommandUpdater* popup_command_updater = popup_browser->command_updater();
  EXPECT_FALSE(popup_command_updater->IsCommandEnabled(IDC_MANAGE_EXTENSIONS));
  EXPECT_FALSE(popup_command_updater->IsCommandEnabled(IDC_OPTIONS));
  EXPECT_TRUE(popup_command_updater->IsCommandEnabled(
      IDC_SHOW_BOOKMARK_MANAGER));
  EXPECT_FALSE(popup_command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));
}

// Makes sure Extensions and Settings commands are disabled in certain
// circumstances even though normally they should stay enabled.
IN_PROC_BROWSER_TEST_F(BrowserTest,
                       DisableOptionsAndImportMenuItemsConsistently) {
  // Create a popup browser.
  Browser* popup_browser = browser()->CreateForType(Browser::TYPE_POPUP,
                                                    browser()->profile());
  CommandUpdater* command_updater = popup_browser->command_updater();
  // OPTIONS and IMPORT_SETTINGS are disabled for a non-normal UI.
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_OPTIONS));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));

  // Set Incognito to FORCED.
  IncognitoModePrefs::SetAvailability(popup_browser->profile()->GetPrefs(),
                                      IncognitoModePrefs::FORCED);
  // OPTIONS and IMPORT_SETTINGS are disabled when Incognito is forced.
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_OPTIONS));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));
  // Set Incognito to AVAILABLE.
  IncognitoModePrefs::SetAvailability(popup_browser->profile()->GetPrefs(),
                                      IncognitoModePrefs::ENABLED);
  // OPTIONS and IMPORT_SETTINGS are still disabled since it is a non-normal UI.
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_OPTIONS));
  EXPECT_FALSE(command_updater->IsCommandEnabled(IDC_IMPORT_SETTINGS));
}

// TODO(ben): this test was never enabled. It has bit-rotted since being added.
// It originally lived in browser_unittest.cc, but has been moved here to make
// room for real browser unit tests.
#if 0
class BrowserTest2 : public InProcessBrowserTest {
 public:
  BrowserTest2() {
    host_resolver_proc_ = new net::RuleBasedHostResolverProc(NULL);
    // Avoid making external DNS lookups. In this test we don't need this
    // to succeed.
    host_resolver_proc_->AddSimulatedFailure("*.google.com");
    scoped_host_resolver_proc_.Init(host_resolver_proc_.get());
  }

 private:
  scoped_refptr<net::RuleBasedHostResolverProc> host_resolver_proc_;
  net::ScopedDefaultHostResolverProc scoped_host_resolver_proc_;
};

IN_PROC_BROWSER_TEST_F(BrowserTest2, NoTabsInPopups) {
  Browser::RegisterAppPrefs(L"Test");

  // We start with a normal browser with one tab.
  EXPECT_EQ(1, browser()->tab_count());

  // Open a popup browser with a single blank foreground tab.
  Browser* popup_browser = browser()->CreateForType(Browser::TYPE_POPUP,
                                                    browser()->profile());
  popup_browser->AddBlankTab(true);
  EXPECT_EQ(1, popup_browser->tab_count());

  // Now try opening another tab in the popup browser.
  AddTabWithURLParams params1(url, content::PAGE_TRANSITION_TYPED);
  popup_browser->AddTabWithURL(&params1);
  EXPECT_EQ(popup_browser, params1.target);

  // The popup should still only have one tab.
  EXPECT_EQ(1, popup_browser->tab_count());

  // The normal browser should now have two.
  EXPECT_EQ(2, browser()->tab_count());

  // Open an app frame browser with a single blank foreground tab.
  Browser* app_browser =
      browser()->CreateForApp(L"Test", browser()->profile(), false);
  app_browser->AddBlankTab(true);
  EXPECT_EQ(1, app_browser->tab_count());

  // Now try opening another tab in the app browser.
  AddTabWithURLParams params2(GURL(chrome::kAboutBlankURL),
                              content::PAGE_TRANSITION_TYPED);
  app_browser->AddTabWithURL(&params2);
  EXPECT_EQ(app_browser, params2.target);

  // The popup should still only have one tab.
  EXPECT_EQ(1, app_browser->tab_count());

  // The normal browser should now have three.
  EXPECT_EQ(3, browser()->tab_count());

  // Open an app frame popup browser with a single blank foreground tab.
  Browser* app_popup_browser =
      browser()->CreateForApp(L"Test", browser()->profile(), false);
  app_popup_browser->AddBlankTab(true);
  EXPECT_EQ(1, app_popup_browser->tab_count());

  // Now try opening another tab in the app popup browser.
  AddTabWithURLParams params3(GURL(chrome::kAboutBlankURL),
                              content::PAGE_TRANSITION_TYPED);
  app_popup_browser->AddTabWithURL(&params3);
  EXPECT_EQ(app_popup_browser, params3.target);

  // The popup should still only have one tab.
  EXPECT_EQ(1, app_popup_browser->tab_count());

  // The normal browser should now have four.
  EXPECT_EQ(4, browser()->tab_count());

  // Close the additional browsers.
  popup_browser->CloseAllTabs();
  app_browser->CloseAllTabs();
  app_popup_browser->CloseAllTabs();
}
#endif
