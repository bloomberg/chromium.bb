// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_interstitial.h"
#include "chrome/browser/supervised_user/supervised_user_navigation_observer.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

using content::InterstitialPage;
using content::NavigationController;
using content::NavigationEntry;
using content::WebContents;

namespace {

class InterstitialPageObserver : public content::WebContentsObserver {
 public:
  InterstitialPageObserver(WebContents* web_contents,
                           const base::Closure& callback)
      : content::WebContentsObserver(web_contents), callback_(callback) {}
  ~InterstitialPageObserver() override {}

  void DidAttachInterstitialPage() override {
     callback_.Run();
  }

 private:
  base::Closure callback_;
};

// Tests filtering for supervised users.
class SupervisedUserTest : public InProcessBrowserTest {
 public:
  // Indicates whether the interstitial should proceed or not.
  enum InterstitialAction {
    INTERSTITIAL_PROCEED,
    INTERSTITIAL_DONTPROCEED,
  };

  SupervisedUserTest() : supervised_user_service_(nullptr) {}
  ~SupervisedUserTest() override {}

  bool ShownPageIsInterstitial(WebContents* tab) {
    EXPECT_FALSE(tab->IsCrashed());
    return tab->ShowingInterstitialPage();
  }

  void SendAccessRequest(WebContents* tab) {
    InterstitialPage* interstitial_page = tab->GetInterstitialPage();
    ASSERT_TRUE(interstitial_page);

    // Get the SupervisedUserInterstitial delegate.
    content::InterstitialPageDelegate* delegate =
        interstitial_page->GetDelegateForTesting();

    // Simulate the click on the "request" button.
    delegate->CommandReceived("\"request\"");
  }

  void GoBack(WebContents* tab) {
    InterstitialPage* interstitial_page = tab->GetInterstitialPage();
    ASSERT_TRUE(interstitial_page);

    // Get the SupervisedUserInterstitial delegate.
    content::InterstitialPageDelegate* delegate =
        interstitial_page->GetDelegateForTesting();

    // Simulate the click on the "back" button.
    delegate->CommandReceived("\"back\"");
  }

 protected:
  void SetUpOnMainThread() override {
    // Set up the SupervisedUserNavigationObserver manually since the profile
    // was not supervised when the browser was created.
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    SupervisedUserNavigationObserver::CreateForWebContents(web_contents);

    supervised_user_service_ =
        SupervisedUserServiceFactory::GetForProfile(browser()->profile());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Enable the test server and remap all URLs to it.
    ASSERT_TRUE(embedded_test_server()->Start());
    std::string host_port = embedded_test_server()->host_port_pair().ToString();
    command_line->AppendSwitchASCII(switches::kHostResolverRules,
        "MAP *.example.com " + host_port + "," +
        "MAP *.new-example.com " + host_port + "," +
        "MAP *.a.com " + host_port);

    command_line->AppendSwitchASCII(switches::kSupervisedUserId, "asdf");
  }

  // Acts like a synchronous call to history's QueryHistory. Modified from
  // history_querying_unittest.cc.
  void QueryHistory(history::HistoryService* history_service,
                    const std::string& text_query,
                    const history::QueryOptions& options,
                    history::QueryResults* results) {
    base::RunLoop run_loop;
    base::CancelableTaskTracker history_task_tracker;
    auto callback = [](history::QueryResults* new_results,
                       base::RunLoop* run_loop,
                       history::QueryResults* results) {
      results->Swap(new_results);
      run_loop->Quit();
    };
    history_service->QueryHistory(base::UTF8ToUTF16(text_query), options,
                                  base::Bind(callback, results, &run_loop),
                                  &history_task_tracker);
    run_loop.Run();  // Will go until ...Complete calls Quit.
  }

  SupervisedUserService* supervised_user_service_;
};

// Tests the filter mode in which all sites are blocked by default.
class SupervisedUserBlockModeTest : public SupervisedUserTest {
 public:
  void SetUpOnMainThread() override {
    SupervisedUserTest::SetUpOnMainThread();

    Profile* profile = browser()->profile();
    SupervisedUserSettingsService* supervised_user_settings_service =
        SupervisedUserSettingsServiceFactory::GetForProfile(profile);
    supervised_user_settings_service->SetLocalSetting(
        supervised_users::kContentPackDefaultFilteringBehavior,
        base::MakeUnique<base::Value>(SupervisedUserURLFilter::BLOCK));
  }
};

class MockTabStripModelObserver : public TabStripModelObserver {
 public:
  explicit MockTabStripModelObserver(TabStripModel* tab_strip)
      : tab_strip_(tab_strip) {
    tab_strip_->AddObserver(this);
  }

  ~MockTabStripModelObserver() {
    tab_strip_->RemoveObserver(this);
  }

  MOCK_METHOD3(TabClosingAt, void(TabStripModel*, content::WebContents*, int));

 private:
  TabStripModel* tab_strip_;
};

// Navigates to a blocked URL.
IN_PROC_BROWSER_TEST_F(SupervisedUserBlockModeTest,
                       SendAccessRequestOnBlockedURL) {
  GURL test_url("http://www.example.com/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ShownPageIsInterstitial(tab));

  SendAccessRequest(tab);

  // TODO(sergiu): Properly check that the access request was sent here.

  GoBack(tab);

  // Make sure that the tab is still there.
  EXPECT_EQ(tab, browser()->tab_strip_model()->GetActiveWebContents());

  EXPECT_FALSE(ShownPageIsInterstitial(tab));
}

// Navigates to a blocked URL in a new tab. We expect the tab to be closed
// automatically on pressing the "back" button on the interstitial.
IN_PROC_BROWSER_TEST_F(SupervisedUserBlockModeTest, OpenBlockedURLInNewTab) {
  TabStripModel* tab_strip = browser()->tab_strip_model();
  WebContents* prev_tab = tab_strip->GetActiveWebContents();

  // Open blocked URL in a new tab.
  GURL test_url("http://www.example.com/simple.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Check that we got the interstitial.
  WebContents* tab = tab_strip->GetActiveWebContents();
  ASSERT_TRUE(ShownPageIsInterstitial(tab));

  // On pressing the "back" button, the new tab should be closed, and we should
  // get back to the previous active tab.
  MockTabStripModelObserver observer(tab_strip);
  base::RunLoop run_loop;
  EXPECT_CALL(observer,
              TabClosingAt(tab_strip, tab, tab_strip->active_index()))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
  GoBack(tab);
  run_loop.Run();
  EXPECT_EQ(prev_tab, tab_strip->GetActiveWebContents());
}

// Tests whether a visit attempt adds a special history entry.
IN_PROC_BROWSER_TEST_F(SupervisedUserBlockModeTest,
                       HistoryVisitRecorded) {
  GURL allowed_url("http://www.example.com/simple.html");

  scoped_refptr<SupervisedUserURLFilter> filter =
      supervised_user_service_->GetURLFilterForUIThread();

  // Set the host as allowed.
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetBooleanWithoutPathExpansion(allowed_url.host(), true);
  SupervisedUserSettingsService* supervised_user_settings_service =
      SupervisedUserSettingsServiceFactory::GetForProfile(
          browser()->profile());
  supervised_user_settings_service->SetLocalSetting(
      supervised_users::kContentPackManualBehaviorHosts, std::move(dict));
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            filter->GetFilteringBehaviorForURL(allowed_url));
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            filter->GetFilteringBehaviorForURL(allowed_url.GetWithEmptyPath()));

  ui_test_utils::NavigateToURL(browser(), allowed_url);

  // Navigate to it and check that we don't get an interstitial.
  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_FALSE(ShownPageIsInterstitial(tab));

  // Navigate to a blocked page and go back on the interstitial.
  GURL blocked_url("http://www.new-example.com/simple.html");
  ui_test_utils::NavigateToURL(browser(), blocked_url);

  tab = browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ShownPageIsInterstitial(tab));
  GoBack(tab);

  // Check that we went back to the first URL and that the manual behaviors
  // have not changed.
  EXPECT_EQ(allowed_url.spec(), tab->GetURL().spec());
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            filter->GetFilteringBehaviorForURL(allowed_url.GetWithEmptyPath()));
  EXPECT_EQ(SupervisedUserURLFilter::BLOCK,
            filter->GetFilteringBehaviorForURL(blocked_url.GetWithEmptyPath()));

  // Query the history entry.
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(browser()->profile(),
                                           ServiceAccessType::EXPLICIT_ACCESS);
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

IN_PROC_BROWSER_TEST_F(SupervisedUserTest, ImmediatelyProceed) {
  GURL test_url("http://www.example.com/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_FALSE(ShownPageIsInterstitial(web_contents));

  // Manually show an interstitial page for a URL that is allowed. This
  // simulates the case where a network request was blocked on the IO thread
  // because a change to the URL filter hadn't been propagated yet.
  bool proceed = false;
  SupervisedUserInterstitial::Show(
      web_contents, test_url, supervised_user_error_page::MANUAL,
      /* initial_page_load = */ true,
      base::Bind(
          [](bool* result_holder, bool result) {
            *result_holder = result;
          },
          &proceed));

  // The interstitial should not appear, and the callback should have been
  // called immediately.
  EXPECT_FALSE(ShownPageIsInterstitial(web_contents));
  EXPECT_TRUE(proceed);
}

IN_PROC_BROWSER_TEST_F(SupervisedUserTest, GoBackOnDontProceed) {
  // We start out at the initial navigation.
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(0, web_contents->GetController().GetCurrentEntryIndex());

  GURL test_url("http://www.example.com/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  ASSERT_FALSE(ShownPageIsInterstitial(web_contents));

  // Set the host as blocked and wait for the interstitial to appear.
  auto dict = base::MakeUnique<base::DictionaryValue>();
  dict->SetBooleanWithoutPathExpansion(test_url.host(), false);
  SupervisedUserSettingsService* supervised_user_settings_service =
      SupervisedUserSettingsServiceFactory::GetForProfile(
          browser()->profile());
  auto message_loop_runner = make_scoped_refptr(new content::MessageLoopRunner);
  InterstitialPageObserver interstitial_observer(
      web_contents, message_loop_runner->QuitClosure());
  supervised_user_settings_service->SetLocalSetting(
      supervised_users::kContentPackManualBehaviorHosts, std::move(dict));

  scoped_refptr<SupervisedUserURLFilter> filter =
      supervised_user_service_->GetURLFilterForUIThread();
  ASSERT_EQ(SupervisedUserURLFilter::BLOCK,
            filter->GetFilteringBehaviorForURL(test_url));

  message_loop_runner->Run();

  InterstitialPage* interstitial_page = web_contents->GetInterstitialPage();
  ASSERT_TRUE(interstitial_page);

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  interstitial_page->DontProceed();
  observer.Wait();

  // We should have gone back to the initial navigation.
  EXPECT_EQ(0, web_contents->GetController().GetCurrentEntryIndex());
}

IN_PROC_BROWSER_TEST_F(SupervisedUserTest, BlockThenUnblock) {
  GURL test_url("http://www.example.com/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_FALSE(ShownPageIsInterstitial(web_contents));

  // Set the host as blocked and wait for the interstitial to appear.
  auto dict = base::MakeUnique<base::DictionaryValue>();
  dict->SetBooleanWithoutPathExpansion(test_url.host(), false);
  SupervisedUserSettingsService* supervised_user_settings_service =
      SupervisedUserSettingsServiceFactory::GetForProfile(
          browser()->profile());
  auto message_loop_runner = make_scoped_refptr(new content::MessageLoopRunner);
  InterstitialPageObserver observer(web_contents,
                                    message_loop_runner->QuitClosure());
  supervised_user_settings_service->SetLocalSetting(
      supervised_users::kContentPackManualBehaviorHosts, std::move(dict));

  scoped_refptr<SupervisedUserURLFilter> filter =
      supervised_user_service_->GetURLFilterForUIThread();
  ASSERT_EQ(SupervisedUserURLFilter::BLOCK,
            filter->GetFilteringBehaviorForURL(test_url));

  message_loop_runner->Run();
  ASSERT_TRUE(ShownPageIsInterstitial(web_contents));

  dict = base::MakeUnique<base::DictionaryValue>();
  dict->SetBooleanWithoutPathExpansion(test_url.host(), true);
  supervised_user_settings_service->SetLocalSetting(
      supervised_users::kContentPackManualBehaviorHosts, std::move(dict));
  ASSERT_EQ(SupervisedUserURLFilter::ALLOW,
            filter->GetFilteringBehaviorForURL(test_url));

  ASSERT_EQ(test_url, web_contents->GetURL());

  EXPECT_FALSE(ShownPageIsInterstitial(web_contents));
}

IN_PROC_BROWSER_TEST_F(SupervisedUserBlockModeTest, Unblock) {
  GURL test_url("http://www.example.com/simple.html");
  ui_test_utils::NavigateToURL(browser(), test_url);

  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(ShownPageIsInterstitial(web_contents));

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());

  // Set the host as allowed.
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetBooleanWithoutPathExpansion(test_url.host(), true);
  SupervisedUserSettingsService* supervised_user_settings_service =
      SupervisedUserSettingsServiceFactory::GetForProfile(
          browser()->profile());
  supervised_user_settings_service->SetLocalSetting(
      supervised_users::kContentPackManualBehaviorHosts, std::move(dict));

  scoped_refptr<SupervisedUserURLFilter> filter =
      supervised_user_service_->GetURLFilterForUIThread();
  EXPECT_EQ(SupervisedUserURLFilter::ALLOW,
            filter->GetFilteringBehaviorForURL(test_url.GetWithEmptyPath()));

  observer.Wait();
  EXPECT_EQ(test_url, web_contents->GetURL());
}

}  // namespace
