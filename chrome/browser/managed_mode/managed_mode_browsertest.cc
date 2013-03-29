// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/managed_mode/managed_mode.h"
#include "chrome/browser/managed_mode/managed_mode_interstitial.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::InterstitialPage;
using content::NavigationController;
using content::NavigationEntry;
using content::WebContents;

namespace {

class InterstitialObserver : public content::WebContentsObserver {
 public:
  InterstitialObserver(content::WebContents* web_contents,
                       const base::Closure& callback)
      : WebContentsObserver(web_contents),
        callback_(callback) {
  }

  virtual void DidDetachInterstitialPage() OVERRIDE {
    callback_.Run();
  }

 private:
  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(InterstitialObserver);
};

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

  ManagedModeBlockModeTest() : managed_user_service_(NULL) {}
  virtual ~ManagedModeBlockModeTest() {}

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

  bool IsPreviewInfobarPresent() {
    return IsInfobarPresent(IDS_MANAGED_MODE_PREVIEW_MESSAGE);
  }

  bool IsAlreadyAddedInfobarPresent() {
    return IsInfobarPresent(IDS_MANAGED_MODE_ALREADY_ADDED_MESSAGE);
  }

  // Checks whether the correct infobar is present.
  bool IsInfobarPresent(int message_id) {
    InfoBarService* service = InfoBarService::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());

    for (unsigned int i = 0; i < service->GetInfoBarCount(); ++i) {
      ConfirmInfoBarDelegate* delegate =
          service->GetInfoBarDelegateAt(i)->AsConfirmInfoBarDelegate();
      if (delegate->GetMessageText() == l10n_util::GetStringUTF16(message_id))
        return true;
    }
    return false;
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
      if (infobar_action == INFOBAR_ALREADY_ADDED)
        ASSERT_TRUE(IsAlreadyAddedInfobarPresent());
      else
        ASSERT_TRUE(IsPreviewInfobarPresent());

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
          confirm_info_bar_delegate->InfoBarDismissed();
          ASSERT_TRUE(confirm_info_bar_delegate->Accept());
          infobar_service->RemoveInfoBar(confirm_info_bar_delegate);
          break;
        case INFOBAR_CANCEL:
          confirm_info_bar_delegate->InfoBarDismissed();
          ASSERT_FALSE(confirm_info_bar_delegate->Cancel());
          infobar_service->RemoveInfoBar(confirm_info_bar_delegate);
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
      scoped_refptr<content::MessageLoopRunner> loop_runner(
          new content::MessageLoopRunner);
      InterstitialObserver observer(tab, loop_runner->QuitClosure());
      interstitial_page->DontProceed();
      loop_runner->Run();
    }
    EXPECT_FALSE(IsPreviewInfobarPresent());
    EXPECT_FALSE(IsAlreadyAddedInfobarPresent());
  }

 protected:
  virtual void SetUpOnMainThread() OVERRIDE {
    Profile* profile = browser()->profile();
    managed_user_service_ = ManagedUserServiceFactory::GetForProfile(profile);
    profile->GetPrefs()->SetBoolean(prefs::kProfileIsManaged, true);
    managed_user_service_->Init();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Enable the test server and remap all URLs to it.
    ASSERT_TRUE(test_server()->Start());
    std::string host_port = test_server()->host_port_pair().ToString();
    command_line->AppendSwitch(switches::kManaged);
    command_line->AppendSwitchASCII(switches::kHostResolverRules,
        "MAP *.example.com " + host_port + "," +
        "MAP *.new-example.com " + host_port + "," +
        "MAP *.a.com " + host_port);
  }

  ManagedUserService* managed_user_service_;
};

// Navigates to a URL which is not in a manual list, clicks preview on the
// interstitial and then allows the website. The website should now be in the
// manual whitelist.
IN_PROC_BROWSER_TEST_F(ManagedModeBlockModeTest, SimpleURLNotInAnyLists) {
  GURL test_url("http://www.example.com/files/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  CheckShownPageIsInterstitial(tab);
  ActOnInterstitialAndInfobar(tab, INTERSTITIAL_PROCEED, INFOBAR_ACCEPT);

  EXPECT_EQ(ManagedUserService::MANUAL_ALLOW,
            managed_user_service_->GetManualBehaviorForHost("www.example.com"));
}

// Same as above just that the URL redirects to a second URL first. The initial
// exact URL should be in the whitelist as well as the second hostname.
IN_PROC_BROWSER_TEST_F(ManagedModeBlockModeTest, RedirectedURLsNotInAnyLists) {
  std::vector<std::string> url_list;
  url_list.push_back("www.a.com");
  std::string last_url("www.example.com/files/simple.html");
  GURL test_url(GetRedirectURL(url_list, last_url));

  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  CheckShownPageIsInterstitial(tab);
  ActOnInterstitialAndInfobar(tab, INTERSTITIAL_PROCEED, INFOBAR_ACCEPT);

  EXPECT_EQ(ManagedUserService::MANUAL_ALLOW,
            managed_user_service_->GetManualBehaviorForURL(
                GURL("http://www.a.com/server-redirect")));
  EXPECT_EQ(ManagedUserService::MANUAL_ALLOW,
            managed_user_service_->GetManualBehaviorForHost("www.example.com"));
}

// Navigates to a URL in the whitelist. No interstitial should be shown and
// the browser should navigate to the page.
IN_PROC_BROWSER_TEST_F(ManagedModeBlockModeTest, SimpleURLInWhitelist) {
  GURL test_url("http://www.example.com/files/simple.html");
  std::vector<std::string> hosts;
  hosts.push_back(test_url.host());
  managed_user_service_->SetManualBehaviorForHosts(
      hosts, ManagedUserService::MANUAL_ALLOW);
  EXPECT_EQ(ManagedUserService::MANUAL_ALLOW,
            managed_user_service_->GetManualBehaviorForHost(test_url.host()));

  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  CheckShownPageIsNotInterstitial(tab);
}

// Navigates to a URL which redirects to another URL, both in the whitelist.
// No interstitial should be shown and the browser should navigate to the page.
IN_PROC_BROWSER_TEST_F(ManagedModeBlockModeTest,
                       RedirectedURLsBothInWhitelist) {
  std::string a_domain = "www.a.com";
  std::vector<std::string> url_list;
  url_list.push_back(a_domain);
  std::string example_domain = "www.example.com";
  std::string last_url = example_domain + "/files/simple.html";
  GURL test_url(GetRedirectURL(url_list, last_url));

  // Add both hostnames to the whitelist, should navigate without interstitial.
  std::vector<std::string> hosts;
  hosts.push_back(a_domain);
  hosts.push_back(example_domain);
  managed_user_service_->SetManualBehaviorForHosts(
      hosts, ManagedUserService::MANUAL_ALLOW);

  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  CheckShownPageIsNotInterstitial(tab);

  EXPECT_EQ(ManagedUserService::MANUAL_ALLOW,
            managed_user_service_->GetManualBehaviorForHost(a_domain));
  EXPECT_EQ(ManagedUserService::MANUAL_ALLOW,
            managed_user_service_->GetManualBehaviorForHost(example_domain));
}

// Only one URL is in the whitelist and the second not, so it should redirect,
// show an interstitial, then after clicking preview and clicking accept both
// websites should be in the whitelist.
IN_PROC_BROWSER_TEST_F(ManagedModeBlockModeTest,
                       RedirectedURLFirstInWhitelist) {
  std::string a_domain = "www.a.com";
  std::vector<std::string> url_list;
  url_list.push_back(a_domain);
  std::string example_domain = "www.example.com";
  std::string last_url = example_domain + "/files/simple.html";
  GURL test_url(GetRedirectURL(url_list, last_url));

  // Add the first URL to the whitelist.
  std::vector<std::string> hosts;
  hosts.push_back(a_domain);
  managed_user_service_->SetManualBehaviorForHosts(
      hosts, ManagedUserService::MANUAL_ALLOW);

  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(tab->GetURL().spec(), "http://" + last_url);
  CheckShownPageIsInterstitial(tab);
  ActOnInterstitialAndInfobar(tab, INTERSTITIAL_PROCEED, INFOBAR_ACCEPT);

  EXPECT_EQ(ManagedUserService::MANUAL_ALLOW,
            managed_user_service_->GetManualBehaviorForHost(a_domain));
  EXPECT_EQ(ManagedUserService::MANUAL_ALLOW,
            managed_user_service_->GetManualBehaviorForHost(example_domain));
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
  std::vector<std::string> hosts;
  hosts.push_back("www.example.com");
  managed_user_service_->SetManualBehaviorForHosts(
      hosts, ManagedUserService::MANUAL_ALLOW);

  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(tab->GetURL().host(), "www.a.com");
  CheckShownPageIsInterstitial(tab);
  ActOnInterstitialAndInfobar(tab, INTERSTITIAL_PROCEED,
                                   INFOBAR_ALREADY_ADDED);

  EXPECT_EQ(ManagedUserService::MANUAL_ALLOW,
            managed_user_service_->GetManualBehaviorForURL(
                GURL("http://www.a.com/server-redirect")));
  EXPECT_EQ(ManagedUserService::MANUAL_ALLOW,
            managed_user_service_->GetManualBehaviorForHost("www.example.com"));
}

// Tests whether going back after being shown an interstitial works. The user
// goes back by using the button on the interstitial. No websites should be
// added to the whitelist.
IN_PROC_BROWSER_TEST_F(ManagedModeBlockModeTest,
                       SimpleURLNotInAnyListsGoBackOnInterstitial) {
  GURL test_url("http://www.example.com/files/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  CheckShownPageIsInterstitial(tab);
  ActOnInterstitialAndInfobar(tab, INTERSTITIAL_DONTPROCEED,
                                   INFOBAR_NOT_USED);

  EXPECT_EQ("about:blank", tab->GetURL().spec());

  EXPECT_EQ(ManagedUserService::MANUAL_NONE,
            managed_user_service_->GetManualBehaviorForHost("www.example.com"));
}

// Tests whether going back after being shown an interstitial works. The user
// previews the page and then goes back by using the button on the infobar. No
// websites should be added to the whitelist.
IN_PROC_BROWSER_TEST_F(ManagedModeBlockModeTest,
                       SimpleURLNotInAnyListsGoBackOnInfobar) {
  GURL test_url("http://www.example.com/files/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  CheckShownPageIsInterstitial(tab);
  ActOnInterstitialAndInfobar(tab, INTERSTITIAL_PROCEED,
                                   INFOBAR_CANCEL);

  EXPECT_EQ("about:blank", tab->GetURL().spec());

  EXPECT_EQ(ManagedUserService::MANUAL_NONE,
            managed_user_service_->GetManualBehaviorForHost("www.example.com"));
}

// Like SimpleURLNotInAnyLists just that it navigates to a page on the allowed
// domain after clicking allow on the infobar. The navigation should complete
// and no interstitial should be shown the second time.
IN_PROC_BROWSER_TEST_F(ManagedModeBlockModeTest,
                       SimpleURLNotInAnyListsNavigateAgain) {
  GURL test_url("http://www.example.com/files/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  CheckShownPageIsInterstitial(tab);
  ActOnInterstitialAndInfobar(tab, INTERSTITIAL_PROCEED, INFOBAR_ACCEPT);

  EXPECT_EQ(ManagedUserService::MANUAL_ALLOW,
            managed_user_service_->GetManualBehaviorForHost(
                "www.example.com"));

  // Navigate to a different page on the same host.
  test_url = GURL("http://www.example.com/files/english_page.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  // An interstitial should not show up.
  CheckShownPageIsNotInterstitial(tab);
  EXPECT_FALSE(IsPreviewInfobarPresent());
}

// Same as above just that it reloads the page instead of navigating to another
// page on the website. Same expected behavior.
IN_PROC_BROWSER_TEST_F(ManagedModeBlockModeTest,
                       SimpleURLNotInAnyListsReloadPageAfterAdd) {
  GURL test_url("http://www.example.com/files/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  CheckShownPageIsInterstitial(tab);
  ActOnInterstitialAndInfobar(tab, INTERSTITIAL_PROCEED, INFOBAR_ACCEPT);

  EXPECT_EQ(ManagedUserService::MANUAL_ALLOW,
            managed_user_service_->GetManualBehaviorForHost("www.example.com"));

  // Reload the page
  tab->GetController().Reload(false);

  // Expect that the page shows up and not an interstitial.
  CheckShownPageIsNotInterstitial(tab);
  EXPECT_FALSE(IsPreviewInfobarPresent());
  EXPECT_EQ(test_url.spec(), tab->GetURL().spec());
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

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

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
  EXPECT_TRUE(IsPreviewInfobarPresent());

  // Navigate to a URL on the same host.
  test_url = GURL("http://www.example.com/files/english_page.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  // Check that the infobar is still there.
  EXPECT_TRUE(IsPreviewInfobarPresent());

  // Navigate to another URL on the same host.
  test_url = GURL("http://www.example.com/files/french_page.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  // Check that the infobar is still there.
  EXPECT_TRUE(IsPreviewInfobarPresent());

  InfoBarDelegate* info_bar_delegate =
      content::Details<InfoBarAddedDetails>(infobar_added.details()).ptr();
  ConfirmInfoBarDelegate* confirm_info_bar_delegate =
      info_bar_delegate->AsConfirmInfoBarDelegate();
  ASSERT_TRUE(confirm_info_bar_delegate);

  content::WindowedNotificationObserver infobar_removed(
      chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_REMOVED,
      content::NotificationService::AllSources());

  // Finally accept the infobar and see that it is gone.
  confirm_info_bar_delegate->InfoBarDismissed();
  ASSERT_TRUE(confirm_info_bar_delegate->Accept());
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(tab);
  infobar_service->RemoveInfoBar(confirm_info_bar_delegate);
  infobar_removed.Wait();

  EXPECT_FALSE(IsPreviewInfobarPresent());

  EXPECT_EQ(ManagedUserService::MANUAL_ALLOW,
            managed_user_service_->GetManualBehaviorForHost("www.example.com"));
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

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

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
  EXPECT_TRUE(IsPreviewInfobarPresent());
  CheckShownPageIsNotInterstitial(tab);

  // Navigate to another URL on a different host.
  test_url = GURL("http://www.new-example.com/files/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  CheckShownPageIsInterstitial(tab);
  EXPECT_FALSE(IsPreviewInfobarPresent());

  ActOnInterstitialAndInfobar(tab, INTERSTITIAL_PROCEED, INFOBAR_ACCEPT);

  EXPECT_EQ(ManagedUserService::MANUAL_NONE,
            managed_user_service_->GetManualBehaviorForHost("www.example.com"));
  EXPECT_EQ(ManagedUserService::MANUAL_ALLOW,
            managed_user_service_->GetManualBehaviorForHost(
                "www.new-example.com"));
}

// Now check that the passphrase dialog is shown when a passphrase is specified
// and the user clicks on the preview button.
IN_PROC_BROWSER_TEST_F(ManagedModeBlockModeTest,
                       PreviewAuthenticationRequired) {
  // Set a passphrase.
  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetString(prefs::kManagedModeLocalPassphrase, "test");

  // Navigate to an URL which should be blocked.
  GURL test_url("http://www.example.com/files/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  InterstitialPage* interstitial_page = tab->GetInterstitialPage();

  // Get the ManagedModeInterstitial delegate.
  content::InterstitialPageDelegate* delegate =
      interstitial_page->GetDelegateForTesting();

  // Simulate the click on the "preview" button.
  delegate->CommandReceived("\"preview\"");
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(tab);
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsShowingDialog());
}

}  // namespace
