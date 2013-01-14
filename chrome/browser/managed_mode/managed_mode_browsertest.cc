// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/managed_mode/managed_mode.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

using content::InterstitialPage;
using content::WebContents;
using content::NavigationController;
using content::NavigationEntry;

// TODO(sergiu): Make the webkit error message disappear when navigating to an
// interstitial page. The message states: "Not allowed to load local resource:
// chrome://resources/css/widgets.css" followed by the compiled page.
class ManagedModeBlockModeTest : public InProcessBrowserTest {
 public:
  // Indicates whether the interstitial should proceed or not.
  enum InterstitialAction {
    INTERSTITIAL_PROCEED,
    INTERSTITIAL_DONTPROCEED,
  };

  // Indicates the infobar action or expected state. INFOBAR_ACCEPT and
  // INFOBAR_CANCEL act on the infobar. INFOBAR_ALREADY_ADDED shows that
  // the "Website was already added infobar" is expected (which expires
  // automatically) and INFOBAR_NOT_USED shows that an infobar is not expected
  // in this case.
  enum InfobarAction {
    INFOBAR_ACCEPT,
    INFOBAR_CANCEL,
    INFOBAR_ALREADY_ADDED,
    INFOBAR_NOT_USED,
  };

  ManagedModeBlockModeTest() {}

  // Builds the redirect URL for the testserver from the hostnames and the
  // final URL and returns it as a string.
  std::string GetRedirectURL(std::vector<std::string> hostnames,
                             std::string final_url) {
    // TODO(sergiu): Figure out how to make multiple successive redirects
    // trigger multiple notfications to test more advanced scenarios.
    std::string output;
    for (std::vector<std::string>::const_iterator it = hostnames.begin();
         it != hostnames.end(); ++it) {
      output += "http://" + *it + "/server-redirect?";
    }

    output += "http://" + final_url;
    return output;
  }

  void CheckShownPageIsInterstitial(WebContents* tab) {
    CheckShownPage(tab, content::PAGE_TYPE_INTERSTITIAL);
  }

  void CheckShownPageIsNotInterstitial(WebContents* tab) {
    CheckShownPage(tab, content::PAGE_TYPE_NORMAL);
  }

  // Checks to see if the type of the current page is |page_type|.
  void CheckShownPage(WebContents* tab, content::PageType page_type) {
    ASSERT_FALSE(tab->IsCrashed());
    NavigationEntry* entry = tab->GetController().GetActiveEntry();
    ASSERT_TRUE(entry);
    ASSERT_EQ(page_type, entry->GetPageType());
  }

  // Checks if the current number of shown infobars is equal to |expected|.
  void CheckNumberOfInfobars(unsigned int expected) {
    EXPECT_EQ(expected,
        InfoBarService::FromWebContents(
            chrome::GetActiveWebContents(browser()))->GetInfoBarCount());
  }

  // Acts on the interstitial and infobar according to the values set to
  // |interstitial_action| and |infobar_action|.
  void ActOnInterstitialAndInfobar(WebContents* tab,
                                   InterstitialAction interstitial_action,
                                   InfobarAction infobar_action) {
    InterstitialPage* interstitial_page = tab->GetInterstitialPage();
    ASSERT_TRUE(interstitial_page);

    content::WindowedNotificationObserver infobar_added(
        chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
        content::NotificationService::AllSources());

    if (interstitial_action == INTERSTITIAL_PROCEED) {
      content::WindowedNotificationObserver observer(
          content::NOTIFICATION_LOAD_STOP,
          content::Source<NavigationController>(&tab->GetController()));
      interstitial_page->Proceed();
      observer.Wait();
      infobar_added.Wait();
      CheckNumberOfInfobars(1u);

      InfoBarDelegate* info_bar_delegate =
          content::Details<InfoBarAddedDetails>(infobar_added.details()).ptr();
      ConfirmInfoBarDelegate* confirm_info_bar_delegate =
          info_bar_delegate->AsConfirmInfoBarDelegate();
      ASSERT_TRUE(confirm_info_bar_delegate);

      content::WindowedNotificationObserver infobar_removed(
          chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
          content::NotificationService::AllSources());
      InfoBarService* infobar_service =
          InfoBarService::FromWebContents(tab);

      switch (infobar_action) {
        case INFOBAR_ACCEPT:
          confirm_info_bar_delegate->Accept();
          break;
        case INFOBAR_CANCEL:
          confirm_info_bar_delegate->Cancel();
          break;
        case INFOBAR_ALREADY_ADDED:
          confirm_info_bar_delegate->InfoBarDismissed();
          infobar_service->RemoveInfoBar(confirm_info_bar_delegate);
          break;
        case INFOBAR_NOT_USED:
          NOTREACHED();
          break;
      }

      infobar_removed.Wait();
    } else {
      content::WindowedNotificationObserver observer(
          content::NOTIFICATION_INTERSTITIAL_DETACHED,
          content::Source<WebContents>(tab));
      interstitial_page->DontProceed();
      observer.Wait();
    }
    CheckNumberOfInfobars(0);
  }

 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) {
    // Enable the test server and remap all URLs to it.
    ASSERT_TRUE(test_server()->Start());
    std::string host_port = test_server()->host_port_pair().ToString();
    command_line->AppendSwitch(switches::kManaged);
    command_line->AppendSwitchASCII(switches::kHostResolverRules,
        "MAP *.example.com " + host_port + "," +
        "MAP *.new-example.com " + host_port + "," +
        "MAP *.a.com " + host_port);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ManagedModeBlockModeTest);
};

// Navigates to a URL which is not in a manual list, clicks preview on the
// interstitial and then allows the website. The website should now be in the
// manual whitelist.
IN_PROC_BROWSER_TEST_F(ManagedModeBlockModeTest, SimpleURLNotInAnyLists) {
  GURL test_url("http://www.example.com/files/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = chrome::GetActiveWebContents(browser());

  CheckShownPageIsInterstitial(tab);
  ActOnInterstitialAndInfobar(tab, INTERSTITIAL_PROCEED, INFOBAR_ACCEPT);

  EXPECT_TRUE(ManagedMode::IsInManualList(true, "www.example.com"));
}

// Same as above just that the URL redirects to a second URL first. The initial
// exact URL should be in the whitelist as well as the second hostname.
IN_PROC_BROWSER_TEST_F(ManagedModeBlockModeTest, RedirectedURLsNotInAnyLists) {
  std::vector<std::string> url_list;
  url_list.push_back("www.a.com");
  std::string last_url("www.example.com/files/simple.html");
  GURL test_url(GetRedirectURL(url_list, last_url));

  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = chrome::GetActiveWebContents(browser());

  CheckShownPageIsInterstitial(tab);
  ActOnInterstitialAndInfobar(tab, INTERSTITIAL_PROCEED, INFOBAR_ACCEPT);

  EXPECT_TRUE(ManagedMode::IsInManualList(true,
      "http://.www.a.com/server-redirect"));
  EXPECT_TRUE(ManagedMode::IsInManualList(true, "www.example.com"));
}

// Navigates to a URL in the whitelist. No interstitial should be shown and
// the browser should navigate to the page.
IN_PROC_BROWSER_TEST_F(ManagedModeBlockModeTest, SimpleURLInWhitelist) {
  GURL test_url("http://www.example.com/files/simple.html");
  ListValue whitelist;
  whitelist.AppendString(test_url.host());
  ManagedMode::AddToManualList(true, whitelist);

  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = chrome::GetActiveWebContents(browser());

  CheckShownPageIsNotInterstitial(tab);

  EXPECT_TRUE(ManagedMode::IsInManualList(true, "www.example.com"));
}

// Navigates to a URL which redirects to another URL, both in the whitelist.
// No interstitial should be shown and the browser should navigate to the page.
IN_PROC_BROWSER_TEST_F(ManagedModeBlockModeTest,
                       RedirectedURLsBothInWhitelist) {
  std::vector<std::string> url_list;
  url_list.push_back("www.a.com");
  std::string last_url("www.example.com/files/simple.html");
  GURL test_url(GetRedirectURL(url_list, last_url));

  // Add both hostnames to the whitelist, should navigate without interstitial.
  ListValue whitelist;
  whitelist.AppendString("www.a.com");
  whitelist.AppendString("www.example.com");
  ManagedMode::AddToManualList(true, whitelist);

  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = chrome::GetActiveWebContents(browser());

  CheckShownPageIsNotInterstitial(tab);

  EXPECT_TRUE(ManagedMode::IsInManualList(true, "www.a.com"));
  EXPECT_TRUE(ManagedMode::IsInManualList(true, "www.example.com"));
}

// Only one URL is in the whitelist and the second not, so it should redirect,
// show an interstitial, then after clicking preview and clicking accept both
// websites should be in the whitelist.
IN_PROC_BROWSER_TEST_F(ManagedModeBlockModeTest,
                       RedirectedURLFirstInWhitelist) {
  std::vector<std::string> url_list;
  url_list.push_back("www.a.com");
  std::string last_url("www.example.com/files/simple.html");
  GURL test_url(GetRedirectURL(url_list, last_url));

  // Add the first URL to the whitelist.
  ListValue whitelist;
  whitelist.AppendString("www.a.com");
  ManagedMode::AddToManualList(true, whitelist);

  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = chrome::GetActiveWebContents(browser());

  EXPECT_EQ(tab->GetURL().spec(), "http://" + last_url);
  CheckShownPageIsInterstitial(tab);
  ActOnInterstitialAndInfobar(tab, INTERSTITIAL_PROCEED, INFOBAR_ACCEPT);

  EXPECT_TRUE(ManagedMode::IsInManualList(true, "www.a.com"));
  EXPECT_TRUE(ManagedMode::IsInManualList(true, "www.example.com"));
}

// This test navigates to a URL which is not in the whitelist but redirects to
// one that is. The expected behavior is that the user will get an interstitial
// and after clicking preview the user will see an infobar stating that the URL
// was already in the whitelist (and the first URL gets added as well).
IN_PROC_BROWSER_TEST_F(ManagedModeBlockModeTest,
                       RedirectedURLLastInWhitelist) {
  std::vector<std::string> url_list;
  url_list.push_back("www.a.com");
  std::string last_url("www.example.com/files/simple.html");
  GURL test_url(GetRedirectURL(url_list, last_url));

  // Add the last URL to the whitelist.
  ListValue whitelist;
  whitelist.AppendString("www.example.com");
  ManagedMode::AddToManualList(true, whitelist);

  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = chrome::GetActiveWebContents(browser());

  EXPECT_EQ(tab->GetURL().host(), "www.a.com");
  CheckShownPageIsInterstitial(tab);
  ActOnInterstitialAndInfobar(tab, INTERSTITIAL_PROCEED,
                                   INFOBAR_ALREADY_ADDED);

  EXPECT_TRUE(ManagedMode::IsInManualList(true,
      "http://.www.a.com/server-redirect"));
  EXPECT_TRUE(ManagedMode::IsInManualList(true, "www.example.com"));
}

// Tests whether going back after being shown an interstitial works. No
// websites should be added to the whitelist.
IN_PROC_BROWSER_TEST_F(ManagedModeBlockModeTest,
                       SimpleURLNotInAnyListsGoBack) {
  GURL test_url("http://www.example.com/files/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = chrome::GetActiveWebContents(browser());

  CheckShownPageIsInterstitial(tab);
  ActOnInterstitialAndInfobar(tab, INTERSTITIAL_DONTPROCEED,
                                   INFOBAR_NOT_USED);

  EXPECT_EQ(tab->GetURL().spec(), "about:blank");

  EXPECT_FALSE(ManagedMode::IsInManualList(true, "www.example.com"));
}

// Like SimpleURLNotInAnyLists just that it navigates to a page on the allowed
// domain after clicking allow on the infobar. The navigation should complete
// and no interstitial should be shown the second time.
IN_PROC_BROWSER_TEST_F(ManagedModeBlockModeTest,
                       SimpleURLNotInAnyListsNavigateAgain) {
  GURL test_url("http://www.example.com/files/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = chrome::GetActiveWebContents(browser());

  CheckShownPageIsInterstitial(tab);
  ActOnInterstitialAndInfobar(tab, INTERSTITIAL_PROCEED, INFOBAR_ACCEPT);

  EXPECT_TRUE(ManagedMode::IsInManualList(true, "www.example.com"));

  // Navigate to a different page on the same host.
  test_url = GURL("http://www.example.com/files/english_page.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  // An interstitial should not show up.
  CheckShownPageIsNotInterstitial(tab);
  CheckNumberOfInfobars(0);
}

// Same as above just that it reloads the page instead of navigating to another
// page on the website. Same expected behavior.
IN_PROC_BROWSER_TEST_F(ManagedModeBlockModeTest,
                       SimpleURLNotInAnyListsReloadPageAfterAdd) {
  GURL test_url("http://www.example.com/files/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = chrome::GetActiveWebContents(browser());

  CheckShownPageIsInterstitial(tab);
  ActOnInterstitialAndInfobar(tab, INTERSTITIAL_PROCEED, INFOBAR_ACCEPT);

  EXPECT_TRUE(ManagedMode::IsInManualList(true, "www.example.com"));

  // Reload the page
  tab->GetController().Reload(false);

  // Expect that the page shows up and not an interstitial.
  CheckShownPageIsNotInterstitial(tab);
  CheckNumberOfInfobars(0);
  EXPECT_EQ(tab->GetURL().spec(), test_url.spec());
}

// Navigates to an HTTPS page not in any lists, similar to the simple test but
// over HTTPS (expected behavior is that only the HTTPS version of the site
// gets whitelisted, compared to the simple case where all protocols get
// whitelisted).
IN_PROC_BROWSER_TEST_F(ManagedModeBlockModeTest,
                       SimpleHTTPSURLNotInAnyLists) {
  // Navigate to an HTTPS URL.
  GURL test_url("https://www.example.com/files/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = chrome::GetActiveWebContents(browser());

  CheckShownPageIsInterstitial(tab);
  ActOnInterstitialAndInfobar(tab, INTERSTITIAL_PROCEED, INFOBAR_ACCEPT);

  // Check that the https:// version is added in the whitelist.
  EXPECT_TRUE(ManagedMode::IsInManualList(true, "https://www.example.com"));
}

// The test navigates to a page, the interstitial is shown and preview is
// clicked but then the test navigates to other pages on the same domain before
// clicking allow on the page. The "allow" infobar should still be there during
// this time and the navigation should be allowed. When the test finally clicks
// accept the webpage should be whitelisted.
IN_PROC_BROWSER_TEST_F(ManagedModeBlockModeTest,
                       SimpleURLNotInAnyListNavigateAround) {
  GURL test_url("http://www.example.com/files/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = chrome::GetActiveWebContents(browser());

  CheckShownPageIsInterstitial(tab);

  InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  ASSERT_TRUE(interstitial_page);

  content::WindowedNotificationObserver infobar_added(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
      content::NotificationService::AllSources());

  // Proceed with the interstitial.
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(&tab->GetController()));
  interstitial_page->Proceed();
  observer.Wait();

  // Wait for the infobar and check that it is there.
  infobar_added.Wait();
  CheckNumberOfInfobars(1u);

  // Navigate to a URL on the same host.
  test_url = GURL("http://www.example.com/files/english_page.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  // Check that the infobar is still there.
  CheckNumberOfInfobars(1u);

  // Navigate to another URL on the same host.
  test_url = GURL("http://www.example.com/files/french_page.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  // Check that the infobar is still there.
  CheckNumberOfInfobars(1u);

  InfoBarDelegate* info_bar_delegate =
      content::Details<InfoBarAddedDetails>(infobar_added.details()).ptr();
  ConfirmInfoBarDelegate* confirm_info_bar_delegate =
      info_bar_delegate->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(confirm_info_bar_delegate);

  content::WindowedNotificationObserver infobar_removed(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
      content::NotificationService::AllSources());

  // Finally accept the infobar and see that it is gone.
  confirm_info_bar_delegate->Accept();
  infobar_removed.Wait();

  CheckNumberOfInfobars(0);

  EXPECT_TRUE(ManagedMode::IsInManualList(true, "www.example.com"));
}

// The test navigates to a page, the interstitial is shown and preview is
// clicked but then the test navigates to a page on a different host, which
// should trigger an interstitial again. Clicking preview on this interstitial
// and accepting it should add the second website to the whitelist and not the
// first one.
IN_PROC_BROWSER_TEST_F(ManagedModeBlockModeTest,
                       NavigateDifferentHostAfterPreview) {
  GURL test_url("http://www.example.com/files/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = chrome::GetActiveWebContents(browser());

  CheckShownPageIsInterstitial(tab);

  InterstitialPage* interstitial_page = tab->GetInterstitialPage();
  ASSERT_TRUE(interstitial_page);

  content::WindowedNotificationObserver infobar_added(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
      content::NotificationService::AllSources());

  // Proceed with the interstitial.
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(&tab->GetController()));
  interstitial_page->Proceed();
  observer.Wait();

  // Wait for the infobar and check that it is there.
  infobar_added.Wait();
  CheckNumberOfInfobars(1u);
  CheckShownPageIsNotInterstitial(tab);

  // Navigate to another URL on a different host.
  test_url = GURL("http://www.new-example.com/files/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  CheckShownPageIsInterstitial(tab);
  CheckNumberOfInfobars(0);

  ActOnInterstitialAndInfobar(tab, INTERSTITIAL_PROCEED, INFOBAR_ACCEPT);

  EXPECT_FALSE(ManagedMode::IsInManualList(true, "www.example.com"));
  EXPECT_TRUE(ManagedMode::IsInManualList(true, "www.new-example.com"));
}
