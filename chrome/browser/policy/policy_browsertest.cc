// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "base/test/test_file_util.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/mock_configuration_policy_provider.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_utils.h"
#include "content/test/net/url_request_mock_http_job.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "policy/policy_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using testing::Return;

namespace policy {

namespace {

const char kURL[] = "http://example.com";
const char kCookieValue[] = "converted=true";
// Assigned to Philip J. Fry to fix eventually.
const char kCookieOptions[] = ";expires=Wed Jan 01 3000 00:00:00 GMT";

// Filters requests to the hosts in |urls| and redirects them to the test data
// dir through URLRequestMockHTTPJobs.
void RedirectHostsToTestDataOnIOThread(const GURL* const urls[], size_t size) {
  // Map the given hosts to the test data dir.
  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  for (size_t i = 0; i < size; ++i) {
    const GURL* url = urls[i];
    EXPECT_TRUE(url->is_valid());
    filter->AddHostnameHandler(url->scheme(), url->host(),
                               URLRequestMockHTTPJob::Factory);
  }
}

// Verifies that the given |url| can be opened. This assumes that |url| points
// at empty.html in the test data dir.
void CheckCanOpenURL(Browser* browser, const GURL& url) {
  ui_test_utils::NavigateToURL(browser, url);
  content::WebContents* contents = chrome::GetActiveWebContents(browser);
  EXPECT_EQ(url, contents->GetURL());
  EXPECT_EQ(net::FormatUrl(url, std::string()), contents->GetTitle());
}

// Verifies that access to the given |url| is blocked.
void CheckURLIsBlocked(Browser* browser, const GURL& url) {
  ui_test_utils::NavigateToURL(browser, url);
  content::WebContents* contents = chrome::GetActiveWebContents(browser);
  EXPECT_EQ(url, contents->GetURL());
  string16 title = UTF8ToUTF16(url.spec() + " is not available");
  EXPECT_EQ(title, contents->GetTitle());

  // Verify that the expected error page is being displayed.
  // (error 138 == NETWORK_ACCESS_DENIED)
  bool result = false;
  EXPECT_TRUE(content::ExecuteJavaScriptAndExtractBool(
      contents->GetRenderViewHost(),
      std::wstring(),
      ASCIIToWide(
          "var hasError = false;"
          "var error = document.getElementById('errorDetails');"
          "if (error)"
          "  hasError = error.textContent.indexOf('Error 138') == 0;"
          "domAutomationController.send(hasError);"),
      &result));
  EXPECT_TRUE(result);
}

void SendToOmniboxAndSubmit(LocationBar* location_bar, const string16& input) {
  OmniboxView* omnibox = location_bar->GetLocationEntry();
  omnibox->model()->OnSetFocus(false);
  omnibox->SetUserText(input);
  location_bar->AcceptInput();
  while (!omnibox->model()->autocomplete_controller()->done()) {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_AUTOCOMPLETE_CONTROLLER_RESULT_READY,
        content::NotificationService::AllSources());
    observer.Wait();
  }
}

// Downloads a file named |file| and expects it to be saved to |dir|, which
// must be empty.
void DownloadAndVerifyFile(
    Browser* browser, const FilePath& dir, const FilePath& file) {
  content::DownloadManager* download_manager =
      content::BrowserContext::GetDownloadManager(browser->profile());
  content::DownloadTestObserverTerminal observer(
      download_manager, 1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  GURL url(URLRequestMockHTTPJob::GetMockUrl(file));
  FilePath downloaded = dir.Append(file);
  EXPECT_FALSE(file_util::PathExists(downloaded));
  ui_test_utils::NavigateToURLWithDisposition(
      browser, url, CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  observer.WaitForFinished();
  EXPECT_EQ(
      1u, observer.NumDownloadsSeenInState(content::DownloadItem::COMPLETE));
  EXPECT_TRUE(file_util::PathExists(downloaded));
  file_util::FileEnumerator enumerator(
      dir, false, file_util::FileEnumerator::FILES);
  EXPECT_EQ(file, enumerator.Next().BaseName());
  EXPECT_EQ(FilePath(), enumerator.Next());
}

}  // namespace

class PolicyTest : public InProcessBrowserTest {
 protected:
  PolicyTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    EXPECT_CALL(provider_, IsInitializationComplete())
        .WillRepeatedly(Return(true));
    BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(chrome_browser_net::SetUrlRequestMocksEnabled, true));
  }

  MockConfigurationPolicyProvider provider_;
};

IN_PROC_BROWSER_TEST_F(PolicyTest, BookmarkBarEnabled) {
  // Verifies that the bookmarks bar can be forced to always or never show up.

  // Test starts in about:blank.
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kShowBookmarkBar));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kShowBookmarkBar));
  EXPECT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());

  PolicyMap policies;
  policies.Set(key::kBookmarkBarEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true));
  provider_.UpdateChromePolicy(policies);
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kShowBookmarkBar));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kShowBookmarkBar));
  EXPECT_EQ(BookmarkBar::SHOW, browser()->bookmark_bar_state());

  // The NTP has special handling of the bookmark bar.
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab"));
  EXPECT_EQ(BookmarkBar::SHOW, browser()->bookmark_bar_state());

  policies.Set(key::kBookmarkBarEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(false));
  provider_.UpdateChromePolicy(policies);
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kShowBookmarkBar));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kShowBookmarkBar));
  // The bookmark bar is hidden in the NTP when disabled by policy.
  EXPECT_EQ(BookmarkBar::HIDDEN, browser()->bookmark_bar_state());

  policies.Clear();
  provider_.UpdateChromePolicy(policies);
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kShowBookmarkBar));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kShowBookmarkBar));
  // The bookmark bar is shown detached in the NTP, when disabled by prefs only.
  EXPECT_EQ(BookmarkBar::DETACHED, browser()->bookmark_bar_state());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, PRE_PRE_ClearSiteDataOnExit) {
  // Verifies that cookies are deleted on shutdown. This test is split in 3
  // parts because it spans 2 browser restarts.

  Profile* profile = browser()->profile();
  GURL url(kURL);
  // No cookies at startup.
  EXPECT_TRUE(content::GetCookies(profile, url).empty());
  // Set a cookie now.
  std::string value = std::string(kCookieValue) + std::string(kCookieOptions);
  EXPECT_TRUE(content::SetCookie(profile, url, value));
  // Verify it was set.
  EXPECT_EQ(kCookieValue, GetCookies(profile, url));
}

IN_PROC_BROWSER_TEST_F(PolicyTest, PRE_ClearSiteDataOnExit) {
  // Verify that the cookie persists across restarts.
  EXPECT_EQ(kCookieValue, GetCookies(browser()->profile(), GURL(kURL)));
  // Now set the policy and the cookie should be gone after another restart.
  PolicyMap policies;
  policies.Set(key::kClearSiteDataOnExit, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true));
  provider_.UpdateChromePolicy(policies);
}

IN_PROC_BROWSER_TEST_F(PolicyTest, ClearSiteDataOnExit) {
  // Verify that the cookie is gone.
  EXPECT_TRUE(GetCookies(browser()->profile(), GURL(kURL)).empty());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, DefaultSearchProvider) {
  // Verifies that a default search is made using the provider configured via
  // policy. Also checks that default search can be completely disabled.
  const string16 kKeyword(ASCIIToUTF16("testsearch"));
  const std::string kSearchURL("http://search.example/search?q={searchTerms}");

  TemplateURLService* service = TemplateURLServiceFactory::GetForProfile(
      browser()->profile());
  ui_test_utils::WaitForTemplateURLServiceToLoad(service);
  TemplateURL* default_search = service->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search);
  EXPECT_NE(kKeyword, default_search->keyword());
  EXPECT_NE(kSearchURL, default_search->url());

  // Override the default search provider using policies.
  PolicyMap policies;
  policies.Set(key::kDefaultSearchProviderEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true));
  policies.Set(key::kDefaultSearchProviderKeyword, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateStringValue(kKeyword));
  policies.Set(key::kDefaultSearchProviderSearchURL, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateStringValue(kSearchURL));
  provider_.UpdateChromePolicy(policies);
  default_search = service->GetDefaultSearchProvider();
  ASSERT_TRUE(default_search);
  EXPECT_EQ(kKeyword, default_search->keyword());
  EXPECT_EQ(kSearchURL, default_search->url());

  // Verify that searching from the omnibox uses kSearchURL.
  chrome::FocusLocationBar(browser());
  LocationBar* location_bar = browser()->window()->GetLocationBar();
  SendToOmniboxAndSubmit(location_bar, ASCIIToUTF16("stuff to search for"));
  OmniboxEditModel* model = location_bar->GetLocationEntry()->model();
  EXPECT_TRUE(model->CurrentMatch().destination_url.is_valid());
  content::WebContents* web_contents = chrome::GetActiveWebContents(browser());
  GURL expected("http://search.example/search?q=stuff+to+search+for");
  EXPECT_EQ(expected, web_contents->GetURL());

  // Verify that searching from the omnibox can be disabled.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  policies.Set(key::kDefaultSearchProviderEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(false));
  EXPECT_TRUE(service->GetDefaultSearchProvider());
  provider_.UpdateChromePolicy(policies);
  EXPECT_FALSE(service->GetDefaultSearchProvider());
  SendToOmniboxAndSubmit(location_bar, ASCIIToUTF16("should not work"));
  // This means that submitting won't trigger any action.
  EXPECT_FALSE(model->CurrentMatch().destination_url.is_valid());
  EXPECT_EQ(GURL("about:blank"), web_contents->GetURL());
}

// This policy isn't available on Chrome OS.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PolicyTest, DownloadDirectory) {
  // Verifies that the download directory can be forced by policy.

  // Set the initial download directory.
  ScopedTempDir initial_dir;
  ASSERT_TRUE(initial_dir.CreateUniqueTempDir());
  browser()->profile()->GetPrefs()->SetFilePath(
      prefs::kDownloadDefaultDirectory, initial_dir.path());
  // Don't prompt for the download location during this test.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kPromptForDownload, false);

  // Verify that downloads end up on the default directory.
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  DownloadAndVerifyFile(browser(), initial_dir.path(), file);
  file_util::DieFileDie(initial_dir.path().Append(file), false);

  // Override the download directory with the policy and verify a download.
  ScopedTempDir forced_dir;
  ASSERT_TRUE(forced_dir.CreateUniqueTempDir());
  PolicyMap policies;
  policies.Set(key::kDownloadDirectory, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER,
               base::Value::CreateStringValue(forced_dir.path().value()));
  provider_.UpdateChromePolicy(policies);
  DownloadAndVerifyFile(browser(), forced_dir.path(), file);
  // Verify that the first download location wasn't affected.
  EXPECT_FALSE(file_util::PathExists(initial_dir.path().Append(file)));
}
#endif

IN_PROC_BROWSER_TEST_F(PolicyTest, IncognitoEnabled) {
  // Verifies that incognito windows can't be opened when disabled by policy.

  // Disable incognito via policy and verify that incognito windows can't be
  // opened.
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_FALSE(BrowserList::IsOffTheRecordSessionActive());
  PolicyMap policies;
  policies.Set(key::kIncognitoEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(false));
  provider_.UpdateChromePolicy(policies);
  EXPECT_FALSE(chrome::ExecuteCommand(browser(), IDC_NEW_INCOGNITO_WINDOW));
  EXPECT_EQ(1u, BrowserList::size());
  EXPECT_FALSE(BrowserList::IsOffTheRecordSessionActive());

  // Enable via policy and verify that incognito windows can be opened.
  policies.Set(key::kIncognitoEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true));
  provider_.UpdateChromePolicy(policies);
  EXPECT_TRUE(chrome::ExecuteCommand(browser(), IDC_NEW_INCOGNITO_WINDOW));
  EXPECT_EQ(2u, BrowserList::size());
  EXPECT_TRUE(BrowserList::IsOffTheRecordSessionActive());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, SavingBrowserHistoryDisabled) {
  // Verifies that browsing history is not saved.
  PolicyMap policies;
  policies.Set(key::kSavingBrowserHistoryDisabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true));
  provider_.UpdateChromePolicy(policies);
  GURL url = ui_test_utils::GetTestUrl(
      FilePath(FilePath::kCurrentDirectory),
      FilePath(FILE_PATH_LITERAL("empty.html")));
  ui_test_utils::NavigateToURL(browser(), url);
  // Verify that the navigation wasn't saved in the history.
  ui_test_utils::HistoryEnumerator enumerator1(browser()->profile());
  EXPECT_EQ(0u, enumerator1.urls().size());

  // Now flip the policy and try again.
  policies.Set(key::kSavingBrowserHistoryDisabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(false));
  provider_.UpdateChromePolicy(policies);
  ui_test_utils::NavigateToURL(browser(), url);
  // Verify that the navigation was saved in the history.
  ui_test_utils::HistoryEnumerator enumerator2(browser()->profile());
  ASSERT_EQ(1u, enumerator2.urls().size());
  EXPECT_EQ(url, enumerator2.urls()[0]);
}

IN_PROC_BROWSER_TEST_F(PolicyTest, TranslateEnabled) {
  // Verifies that translate can be forced enabled or disabled by policy.

  // Get the |infobar_helper|, and verify that there are no infobars on startup.
  content::WebContents* contents = chrome::GetActiveWebContents(browser());
  ASSERT_TRUE(contents);
  TabContents* tab_contents = TabContents::FromWebContents(contents);
  ASSERT_TRUE(tab_contents);
  InfoBarTabHelper* infobar_helper = tab_contents->infobar_tab_helper();
  ASSERT_TRUE(infobar_helper);
  EXPECT_EQ(0u, infobar_helper->GetInfoBarCount());

  // Force enable the translate feature.
  PolicyMap policies;
  policies.Set(key::kTranslateEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(true));
  provider_.UpdateChromePolicy(policies);
  // Instead of waiting for NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED, this test
  // waits for NOTIFICATION_TAB_LANGUAGE_DETERMINED because that's what the
  // TranslateManager observes. This allows checking that an infobar is NOT
  // shown below, without polling for infobars for some indeterminate amount
  // of time.
  GURL url = ui_test_utils::GetTestUrl(
      FilePath(), FilePath(FILE_PATH_LITERAL("french_page.html")));
  content::WindowedNotificationObserver language_observer1(
      chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(browser(), url);
  language_observer1.Wait();
  // Verify that the translate infobar showed up.
  EXPECT_EQ(1u, infobar_helper->GetInfoBarCount());
  InfoBarDelegate* infobar_delegate = infobar_helper->GetInfoBarDelegateAt(0);
  TranslateInfoBarDelegate* delegate =
      infobar_delegate->AsTranslateInfoBarDelegate();
  ASSERT_TRUE(delegate);
  EXPECT_EQ(TranslateInfoBarDelegate::BEFORE_TRANSLATE, delegate->type());
  EXPECT_EQ("fr", delegate->GetOriginalLanguageCode());

  // Now force disable translate.
  ui_test_utils::CloseAllInfoBars(tab_contents);
  EXPECT_EQ(0u, infobar_helper->GetInfoBarCount());
  policies.Set(key::kTranslateEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, base::Value::CreateBooleanValue(false));
  provider_.UpdateChromePolicy(policies);
  // Navigating to the same URL now doesn't trigger an infobar.
  content::WindowedNotificationObserver language_observer2(
      chrome::NOTIFICATION_TAB_LANGUAGE_DETERMINED,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(browser(), url);
  language_observer2.Wait();
  EXPECT_EQ(0u, infobar_helper->GetInfoBarCount());
}

IN_PROC_BROWSER_TEST_F(PolicyTest, URLBlacklist) {
  // Checks that URLs can be blacklisted, and that exceptions can be made to
  // the blacklist.
  const GURL kAAA("http://aaa.com/empty.html");
  const GURL kBBB("http://bbb.com/empty.html");
  const GURL kSUB_BBB("http://sub.bbb.com/empty.html");
  const GURL kBBB_PATH("http://bbb.com/policy/device_management");
  // Filter |kURLS| on IO thread, so that requests to those hosts end up
  // as URLRequestMockHTTPJobs.
  const GURL* kURLS[] = {
    &kAAA,
    &kBBB,
    &kSUB_BBB,
    &kBBB_PATH,
  };
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO, FROM_HERE,
      base::Bind(RedirectHostsToTestDataOnIOThread, kURLS, arraysize(kURLS)),
      MessageLoop::QuitClosure());
  content::RunMessageLoop();

  // Verify that all the URLs can be opened without a blacklist.
  for (size_t i = 0; i < arraysize(kURLS); ++i)
    CheckCanOpenURL(browser(), *kURLS[i]);

  // Set a blacklist.
  base::ListValue blacklist;
  blacklist.Append(base::Value::CreateStringValue("bbb.com"));
  PolicyMap policies;
  policies.Set(key::kURLBlacklist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, blacklist.DeepCopy());
  provider_.UpdateChromePolicy(policies);
  // All bbb.com URLs are blocked.
  CheckCanOpenURL(browser(), kAAA);
  for (size_t i = 1; i < arraysize(kURLS); ++i)
    CheckURLIsBlocked(browser(), *kURLS[i]);

  // Whitelist some sites of bbb.com.
  base::ListValue whitelist;
  whitelist.Append(base::Value::CreateStringValue("sub.bbb.com"));
  whitelist.Append(base::Value::CreateStringValue("bbb.com/policy"));
  policies.Set(key::kURLWhitelist, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, whitelist.DeepCopy());
  provider_.UpdateChromePolicy(policies);
  CheckCanOpenURL(browser(), kAAA);
  CheckURLIsBlocked(browser(), kBBB);
  CheckCanOpenURL(browser(), kSUB_BBB);
  CheckCanOpenURL(browser(), kBBB_PATH);
}

}  // namespace policy
