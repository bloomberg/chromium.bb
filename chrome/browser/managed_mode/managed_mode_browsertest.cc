// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/common/cancelable_request.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/managed_mode/managed_mode_interstitial.h"
#include "chrome/browser/managed_mode/managed_mode_navigation_observer.h"
#include "chrome/browser/managed_mode/managed_user_constants.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "chrome/browser/managed_mode/managed_user_settings_service.h"
#include "chrome/browser/managed_mode/managed_user_settings_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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
#include "content/public/test/browser_test_utils.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::InterstitialPage;
using content::NavigationController;
using content::NavigationEntry;
using content::WebContents;

namespace {

// Tests the filter mode in which all sites are blocked by default.
class ManagedModeBlockModeTest : public InProcessBrowserTest {
 public:
  // Indicates whether the interstitial should proceed or not.
  enum InterstitialAction {
    INTERSTITIAL_PROCEED,
    INTERSTITIAL_DONTPROCEED,
  };

  ManagedModeBlockModeTest() : managed_user_service_(NULL) {}
  virtual ~ManagedModeBlockModeTest() {}

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

  void SendAccessRequest(WebContents* tab) {
    InterstitialPage* interstitial_page = tab->GetInterstitialPage();
    ASSERT_TRUE(interstitial_page);

    // Get the ManagedModeInterstitial delegate.
    content::InterstitialPageDelegate* delegate =
        interstitial_page->GetDelegateForTesting();

    // Simulate the click on the "request" button.
    delegate->CommandReceived("\"request\"");
  }

  void GoBack(WebContents* tab) {
    InterstitialPage* interstitial_page = tab->GetInterstitialPage();
    ASSERT_TRUE(interstitial_page);

    // Get the ManagedModeInterstitial delegate.
    content::InterstitialPageDelegate* delegate =
        interstitial_page->GetDelegateForTesting();

    // Simulate the click on the "back" button.
    delegate->CommandReceived("\"back\"");
  }

 protected:
  virtual void SetUpOnMainThread() OVERRIDE {
    // Set up the ManagedModeNavigationObserver manually since the profile was
    // not managed when the browser was created.
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ManagedModeNavigationObserver::CreateForWebContents(web_contents);

    Profile* profile = browser()->profile();
    managed_user_service_ = ManagedUserServiceFactory::GetForProfile(profile);
    ManagedUserSettingsService* managed_user_settings_service =
        ManagedUserSettingsServiceFactory::GetForProfile(profile);
    managed_user_settings_service->SetLocalSettingForTesting(
        managed_users::kContentPackDefaultFilteringBehavior,
        scoped_ptr<base::Value>(
            new base::FundamentalValue(ManagedModeURLFilter::BLOCK)));
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Enable the test server and remap all URLs to it.
    ASSERT_TRUE(test_server()->Start());
    std::string host_port = test_server()->host_port_pair().ToString();
    command_line->AppendSwitchASCII(switches::kHostResolverRules,
        "MAP *.example.com " + host_port + "," +
        "MAP *.new-example.com " + host_port + "," +
        "MAP *.a.com " + host_port);

    command_line->AppendSwitchASCII(switches::kManagedUserId, "asdf");
  }

  // Acts like a synchronous call to history's QueryHistory. Modified from
  // history_querying_unittest.cc.
  void QueryHistory(HistoryService* history_service,
                    const std::string& text_query,
                    const history::QueryOptions& options,
                    history::QueryResults* results) {
    CancelableRequestConsumer history_request_consumer;
    base::RunLoop run_loop;
    history_service->QueryHistory(
        UTF8ToUTF16(text_query),
        options,
        &history_request_consumer,
        base::Bind(&ManagedModeBlockModeTest::QueryHistoryComplete,
                   base::Unretained(this),
                   results,
                   &run_loop));
    run_loop.Run();  // Will go until ...Complete calls Quit.
  }

  void QueryHistoryComplete(history::QueryResults* new_results,
                            base::RunLoop* run_loop,
                            HistoryService::Handle /* handle */,
                            history::QueryResults* results) {
    results->Swap(new_results);
    run_loop->Quit();  // Will return out to QueryHistory.
  }

  ManagedUserService* managed_user_service_;
};

// Navigates to a blocked URL.
IN_PROC_BROWSER_TEST_F(ManagedModeBlockModeTest,
                       SendAccessRequestOnBlockedURL) {
  GURL test_url("http://www.example.com/files/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  CheckShownPageIsInterstitial(tab);

  SendAccessRequest(tab);

  // TODO(sergiu): Properly check that the access request was sent here.

  GoBack(tab);

  CheckShownPageIsNotInterstitial(tab);
}

// Tests whether a visit attempt adds a special history entry.
IN_PROC_BROWSER_TEST_F(ManagedModeBlockModeTest,
                       HistoryVisitRecorded) {
  GURL allowed_url("http://www.example.com/files/simple.html");

  // Set the host as allowed.
  scoped_ptr<DictionaryValue> dict(new DictionaryValue);
  dict->SetBooleanWithoutPathExpansion(allowed_url.host(), true);
  ManagedUserSettingsService* managed_user_settings_service =
      ManagedUserSettingsServiceFactory::GetForProfile(
          browser()->profile());
  managed_user_settings_service->SetLocalSettingForTesting(
      managed_users::kContentPackManualBehaviorHosts, dict.PassAs<Value>());
  EXPECT_EQ(
      ManagedUserService::MANUAL_ALLOW,
      managed_user_service_->GetManualBehaviorForHost(allowed_url.host()));

  ui_test_utils::NavigateToURL(browser(), allowed_url);

  // Navigate to it and check that we don't get an interstitial.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  CheckShownPageIsNotInterstitial(tab);

  // Navigate to a blocked page and go back on the interstitial.
  GURL blocked_url("http://www.new-example.com/files/simple.html");
  ui_test_utils::NavigateToURL(browser(), blocked_url);

  tab = browser()->tab_strip_model()->GetActiveWebContents();

  CheckShownPageIsInterstitial(tab);
  GoBack(tab);

  // Check that we went back to the first URL and that the manual behaviors
  // have not changed.
  EXPECT_EQ(allowed_url.spec(), tab->GetURL().spec());
  EXPECT_EQ(ManagedUserService::MANUAL_ALLOW,
            managed_user_service_->GetManualBehaviorForHost("www.example.com"));
  EXPECT_EQ(
      ManagedUserService::MANUAL_NONE,
      managed_user_service_->GetManualBehaviorForHost("www.new-example.com"));

  // Query the history entry.
  HistoryService* history_service = HistoryServiceFactory::GetForProfile(
      browser()->profile(), Profile::EXPLICIT_ACCESS);
  history::QueryOptions options;
  history::QueryResults results;
  QueryHistory(history_service, "", options, &results);

  // Check that the entries have the correct blocked_visit value.
  ASSERT_EQ(2u, results.size());
  EXPECT_EQ(blocked_url.spec(), results[0].url().spec());
  EXPECT_TRUE(results[0].blocked_visit());
  EXPECT_EQ(allowed_url.spec(), results[1].url().spec());
  EXPECT_FALSE(results[1].blocked_visit());
}

IN_PROC_BROWSER_TEST_F(ManagedModeBlockModeTest, Unblock) {
  GURL test_url("http://www.example.com/files/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  CheckShownPageIsInterstitial(web_contents);

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());

  // Set the host as allowed.
  scoped_ptr<DictionaryValue> dict(new DictionaryValue);
  dict->SetBooleanWithoutPathExpansion(test_url.host(), true);
  ManagedUserSettingsService* managed_user_settings_service =
      ManagedUserSettingsServiceFactory::GetForProfile(
          browser()->profile());
  managed_user_settings_service->SetLocalSettingForTesting(
      managed_users::kContentPackManualBehaviorHosts, dict.PassAs<Value>());
  EXPECT_EQ(
      ManagedUserService::MANUAL_ALLOW,
      managed_user_service_->GetManualBehaviorForHost(test_url.host()));

  observer.Wait();
  EXPECT_EQ(test_url, web_contents->GetURL());
}

}  // namespace
