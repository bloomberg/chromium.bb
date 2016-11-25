// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"
#include "url/url_canon.h"

namespace safe_browsing {

const char kSingleFrameTestURL[] =
    "/safe_browsing/download_protection/navigation_observer/"
    "navigation_observer_tests.html";
const char kMultiFrameTestURL[] =
    "/safe_browsing/download_protection/navigation_observer/"
    "navigation_observer_multi_frame_tests.html";
const char kRedirectURL[] =
    "/safe_browsing/download_protection/navigation_observer/redirect.html";
const char kDownloadDataURL[] =
    "data:application/octet-stream;base64,a2poYWxrc2hkbGtoYXNka2xoYXNsa2RoYWxra"
    "GtoYWxza2hka2xzamFoZGxramhhc2xka2hhc2xrZGgKYXNrZGpoa2FzZGpoYWtzaGRrYXNoZGt"
    "oYXNrZGhhc2tkaGthc2hka2Foc2RraGFrc2hka2FzaGRraGFzCmFza2pkaGFrc2hkbSxjbmtza"
    "mFoZGtoYXNrZGhhc2tka2hrYXNkCjg3MzQ2ODEyNzQ2OGtqc2hka2FoZHNrZGhraApha3NqZGt"
    "hc2Roa3NkaGthc2hka2FzaGtkaAohISomXkAqJl4qYWhpZGFzeWRpeWlhc1xcb1wKa2Fqc2Roa"
    "2FzaGRrYXNoZGsKYWtzamRoc2tkaAplbmQK";
const char kIframeDirectDownloadURL[] =
    "/safe_browsing/download_protection/navigation_observer/iframe.html";
const char kIframeRetargetingURL[] =
    "/safe_browsing/download_protection/navigation_observer/"
    "iframe_retargeting.html";
const char kDownloadItemURL[] = "/safe_browsing/download_protection/signed.exe";

// Test class to help create SafeBrowsingNavigationObservers for each
// WebContents before they are actually installed through AttachTabHelper.
class TestNavigationObserverManager
    : public SafeBrowsingNavigationObserverManager {
 public:
  TestNavigationObserverManager() : SafeBrowsingNavigationObserverManager() {
    registrar_.Add(this, chrome::NOTIFICATION_TAB_ADDED,
                   content::NotificationService::AllSources());
  }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    if (type == chrome::NOTIFICATION_TAB_ADDED) {
      content::WebContents* dest_content =
          content::Details<content::WebContents>(details).ptr();
      DCHECK(dest_content);
      observer_list_.push_back(
          new SafeBrowsingNavigationObserver(dest_content, this));
      DCHECK(observer_list_.back());
    } else if (type == chrome::NOTIFICATION_RETARGETING) {
      RecordRetargeting(details);
    }
  }

 protected:
  ~TestNavigationObserverManager() override { observer_list_.clear(); }

 private:
  std::vector<SafeBrowsingNavigationObserver*> observer_list_;
};

class SBNavigationObserverBrowserTest : public InProcessBrowserTest {
 public:
  SBNavigationObserverBrowserTest() {}

  void SetUpOnMainThread() override {
    // Disable Safe Browsing service since it is irrelevant to this test.
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                                 false);
    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("*", "127.0.0.1");
    // Navigate to test page.
    ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(kSingleFrameTestURL));
    observer_manager_ = new TestNavigationObserverManager();
    observer_ = new SafeBrowsingNavigationObserver(
        browser()->tab_strip_model()->GetActiveWebContents(),
        observer_manager_);
    ASSERT_TRUE(observer_);
    ASSERT_TRUE(InitialSetup());
  }

  bool InitialSetup() {
    if (!browser())
      return false;

    if (!downloads_directory_.CreateUniqueTempDir())
      return false;

    // Set up default download path.
    browser()->profile()->GetPrefs()->SetFilePath(
        prefs::kDownloadDefaultDirectory, downloads_directory_.GetPath());
    browser()->profile()->GetPrefs()->SetFilePath(
        prefs::kSaveFileDefaultDirectory, downloads_directory_.GetPath());
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kPromptForDownload,
                                                 false);
    content::DownloadManager* manager =
        content::BrowserContext::GetDownloadManager(browser()->profile());
    DownloadPrefs::FromDownloadManager(manager)->ResetAutoOpen();
    manager->RemoveAllDownloads();

    return true;
  }

  void TearDownOnMainThread() override { delete observer_; }

  // Most test cases will trigger downloads, though we don't really care if
  // download completed or not. So we cancel downloads as soon as we record
  // all the navigation events we need.
  void CancelDownloads() {
    std::vector<content::DownloadItem*> download_items;
    content::DownloadManager* manager =
        content::BrowserContext::GetDownloadManager(browser()->profile());
    manager->GetAllDownloads(&download_items);
    for (auto item : download_items) {
      if (!item->IsDone())
        item->Cancel(true);
    }
  }

  // This function needs javascript support, only works on
  // navigation_observer_tests.html and
  // navigation_observer_multi_frame_tests.html.
  void ClickTestLink(const char* test_name,
                     int number_of_navigations) {
    TabStripModel* tab_strip = browser()->tab_strip_model();
    content::WebContents* current_web_contents =
        tab_strip->GetActiveWebContents();
    ASSERT_TRUE(content::WaitForLoadStop(current_web_contents));
    content::TestNavigationObserver navigation_observer(
      current_web_contents,
      number_of_navigations);
    navigation_observer.StartWatchingNewWebContents();
    // Execute test.
    std::string script = base::StringPrintf("clickLink('%s');", test_name);
    ASSERT_TRUE(content::ExecuteScript(current_web_contents, script));
    // Wait for navigations on current tab and new tab (if any) to finish.
    navigation_observer.Wait();
    navigation_observer.StopWatchingNewWebContents();
    // Cancel unfinished download if any.
    CancelDownloads();
  }

  void VerifyNavigationEvent(const GURL& expected_source_url,
                             const GURL& expected_source_main_frame_url,
                             const GURL& expected_original_request_url,
                             const GURL& expected_destination_url,
                             bool expected_is_user_initiated,
                             bool expected_has_committed,
                             bool expected_has_server_redirect,
                             const NavigationEvent& actual_nav_event) {
    EXPECT_EQ(expected_source_url, actual_nav_event.source_url);
    EXPECT_EQ(expected_source_main_frame_url,
              actual_nav_event.source_main_frame_url);
    EXPECT_EQ(expected_original_request_url,
              actual_nav_event.original_request_url);
    EXPECT_EQ(expected_destination_url, actual_nav_event.destination_url);
    EXPECT_EQ(expected_is_user_initiated, actual_nav_event.is_user_initiated);
    EXPECT_EQ(expected_has_committed, actual_nav_event.has_committed);
    EXPECT_EQ(expected_has_server_redirect,
              actual_nav_event.has_server_redirect);
  }

  void VerifyHostToIpMap() {
    // Since all testing pages have the same host, there is only one entry in
    // host_to_ip_map_.
    SafeBrowsingNavigationObserverManager::HostToIpMap* actual_host_ip_map =
        host_to_ip_map();
    ASSERT_EQ(std::size_t(1), actual_host_ip_map->size());
    auto ip_list =
        actual_host_ip_map->at(embedded_test_server()->base_url().host());
    ASSERT_EQ(std::size_t(1), ip_list.size());
    EXPECT_EQ(embedded_test_server()->host_port_pair().host(),
              ip_list.back().ip);
  }

  SafeBrowsingNavigationObserverManager::NavigationMap* navigation_map() {
    return observer_manager_->navigation_map();
  }

  SafeBrowsingNavigationObserverManager::HostToIpMap* host_to_ip_map() {
    return observer_manager_->host_to_ip_map();
  }

 protected:
  SafeBrowsingNavigationObserverManager* observer_manager_;
  SafeBrowsingNavigationObserver* observer_;

 private:
  base::ScopedTempDir downloads_directory_;
};

// Click on a link and start download on the same page.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest, DirectDownload) {
  ClickTestLink("direct_download", 1);
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  auto nav_map = navigation_map();
  ASSERT_TRUE(nav_map);
  ASSERT_EQ(std::size_t(1), nav_map->size());
  ASSERT_EQ(std::size_t(1), nav_map->at(download_url).size());
  // Since this test uses javascript to mimic clicking on a link (no actual user
  // gesture), and DidGetUserInteraction() does not respond to ExecuteScript(),
  // therefore is_user_initiated is false.
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_map->at(download_url).at(0));
  VerifyHostToIpMap();
}

// Click on a link with rel="noreferrer" attribute, and start download on the
// same page.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       DirectDownloadNoReferrer) {
  ClickTestLink("direct_download_noreferrer", 1);
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  auto nav_map = navigation_map();
  ASSERT_TRUE(nav_map);
  ASSERT_EQ(std::size_t(1), nav_map->size());
  ASSERT_EQ(std::size_t(1), nav_map->at(download_url).size());
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_map->at(download_url).at(0));
  VerifyHostToIpMap();
}

// Click on a link with rel="noreferrer" attribute, and start download in a
// new tab using target=_blank.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       DirectDownloadNoReferrerTargetBlank) {
  ClickTestLink("direct_download_noreferrer_target_blank", 1);
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  auto nav_map = navigation_map();
  ASSERT_TRUE(nav_map);
  ASSERT_EQ(std::size_t(1), nav_map->size());
  ASSERT_EQ(std::size_t(2), nav_map->at(download_url).size());
  // The first NavigationEvent was obtained from NOIFICATION_RETARGETING.
  // TODO(jialiul): After https://crbug.com/651895 is fixed, we'll no longer
  // listen to NOTIFICATION_RETARGETING, hence only one NavigationEvent will
  // be observed with the true initator URL. This applies to other new tab
  // download, and target blank download test cases too.
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_map->at(download_url).at(0));
  // The second one is the actual navigation which triggers download.
  VerifyNavigationEvent(GURL(),        // source_url
                        GURL(),        // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_map->at(download_url).at(1));
  VerifyHostToIpMap();
}

// Click on a link which navigates to a page then redirects to a download using
// META HTTP-EQUIV="refresh". All transitions happen in the same tab.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       SingleMetaRefreshRedirect) {
  ClickTestLink("single_meta_refresh_redirect", 2);
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL redirect_url = embedded_test_server()->GetURL(kRedirectURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  auto nav_map = navigation_map();
  ASSERT_TRUE(nav_map);
  // Since unlike server redirects client redirects commit and then generate a
  // second navigation, our observer records two NavigationEvents for this test.
  ASSERT_EQ(std::size_t(2), nav_map->size());
  ASSERT_EQ(std::size_t(1), nav_map->at(redirect_url).size());
  ASSERT_EQ(std::size_t(1), nav_map->at(download_url).size());
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        redirect_url,  // original_request_url
                        redirect_url,  // destination_url
                        false,         // is_user_initiated,
                        true,          // has_committed
                        false,         // has_server_redirect
                        nav_map->at(redirect_url).at(0));
  VerifyNavigationEvent(redirect_url,  // source_url
                        redirect_url,  // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_map->at(download_url).at(0));
  VerifyHostToIpMap();
}

// https://crbug.com/667784: The test is flaky on Linux.
#if defined(OS_LINUX)
#define MAYBE_SingleMetaRefreshRedirectTargetBlank DISABLED_SingleMetaRefreshRedirectTargetBlank
#else
#define MAYBE_SingleMetaRefreshRedirectTargetBlank SingleMetaRefreshRedirectTargetBlank
#endif
// Click on a link which navigates to a page then redirects to a download using
// META HTTP-EQUIV="refresh". First navigation happens in target blank.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       MAYBE_SingleMetaRefreshRedirectTargetBlank) {
  ClickTestLink("single_meta_refresh_redirect_target_blank", 2);
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL redirect_url = embedded_test_server()->GetURL(kRedirectURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  auto nav_map = navigation_map();
  ASSERT_TRUE(nav_map);
  ASSERT_EQ(std::size_t(2), nav_map->size());
  ASSERT_EQ(std::size_t(2), nav_map->at(redirect_url).size());
  ASSERT_EQ(std::size_t(1), nav_map->at(download_url).size());
  // TODO(jialiul): After https://crbug.com/651895 is fixed, we'll no longer
  // listen to NOTIFICATION_RETARGETING, hence only two NavigationEvents will
  // be observed with the true initator URL.
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        redirect_url,  // original_request_url
                        redirect_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_map->at(redirect_url).at(0));
  VerifyNavigationEvent(GURL(),        // source_url
                        GURL(),        // source_main_frame_url
                        redirect_url,  // original_request_url
                        redirect_url,  // destination_url
                        false,         // is_user_initiated,
                        true,          // has_committed
                        false,         // has_server_redirect
                        nav_map->at(redirect_url).at(1));
  VerifyNavigationEvent(redirect_url,  // source_url
                        redirect_url,  // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_map->at(download_url).at(0));
  VerifyHostToIpMap();
}

// Click on a link which redirects twice before reaching download using
// META HTTP-EQUIV="refresh". All transitions happen in the same tab.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       MultiMetaRefreshRedirects) {
  ClickTestLink("multiple_meta_refresh_redirects", 3);
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL first_redirect_url = embedded_test_server()->GetURL(
      "/safe_browsing/download_protection/navigation_observer/"
      "double_redirect.html");
  GURL second_redirect_url = embedded_test_server()->GetURL(kRedirectURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  auto nav_map = navigation_map();
  ASSERT_TRUE(nav_map);
  ASSERT_EQ(std::size_t(3), nav_map->size());
  ASSERT_EQ(std::size_t(1), nav_map->at(first_redirect_url).size());
  ASSERT_EQ(std::size_t(1), nav_map->at(second_redirect_url).size());
  ASSERT_EQ(std::size_t(1), nav_map->at(download_url).size());
  VerifyNavigationEvent(initial_url,         // source_url
                        initial_url,         // source_main_frame_url
                        first_redirect_url,  // original_request_url
                        first_redirect_url,  // destination_url
                        false,               // is_user_initiated,
                        true,                // has_committed
                        false,               // has_server_redirect
                        nav_map->at(first_redirect_url).at(0));
  VerifyNavigationEvent(first_redirect_url,   // source_url
                        first_redirect_url,   // source_main_frame_url
                        second_redirect_url,  // original_request_url
                        second_redirect_url,  // destination_url
                        false,                // is_user_initiated,
                        true,                 // has_committed
                        false,                // has_server_redirect
                        nav_map->at(second_redirect_url).at(0));
  VerifyNavigationEvent(second_redirect_url,  // source_url
                        second_redirect_url,  // source_main_frame_url
                        download_url,         // original_request_url
                        download_url,         // destination_url
                        false,                // is_user_initiated,
                        false,                // has_committed
                        false,                // has_server_redirect
                        nav_map->at(download_url).at(0));
  VerifyHostToIpMap();
}

// Click on a link which redirects to download using window.location.
// All transitions happen in the same tab.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       WindowLocationRedirect) {
  ClickTestLink("window_location_redirection", 1);
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  auto nav_map = navigation_map();
  ASSERT_TRUE(nav_map);
  ASSERT_EQ(std::size_t(1), nav_map->size());
  ASSERT_EQ(std::size_t(1), nav_map->at(download_url).size());
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_map->at(download_url).at(0));
}

// Click on a link which redirects twice until it reaches download using a
// mixture of meta refresh and window.location. All transitions happen in the
// same tab.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest, MixRedirects) {
  ClickTestLink("mix_redirects", 2);
  GURL redirect_url = embedded_test_server()->GetURL(kRedirectURL);
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  auto nav_map = navigation_map();
  ASSERT_TRUE(nav_map);
  ASSERT_EQ(std::size_t(2), nav_map->size());
  ASSERT_EQ(std::size_t(1), nav_map->at(redirect_url).size());
  ASSERT_EQ(std::size_t(1), nav_map->at(download_url).size());
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        redirect_url,  // original_request_url
                        redirect_url,  // destination_url
                        false,         // is_user_initiated,
                        true,          // has_committed
                        false,         // has_server_redirect
                        nav_map->at(redirect_url).at(0));
  VerifyNavigationEvent(redirect_url,  // source_url
                        redirect_url,  // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_map->at(download_url).at(0));
  VerifyHostToIpMap();
}

// Use javascript to open download in a new tab.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest, NewTabDownload) {
  ClickTestLink("new_tab_download", 2);
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  GURL blank_url = GURL(url::kAboutBlankURL);
  auto nav_map = navigation_map();
  ASSERT_TRUE(nav_map);
  ASSERT_EQ(std::size_t(2), nav_map->size());
  ASSERT_EQ(std::size_t(2), nav_map->at(blank_url).size());
  ASSERT_EQ(std::size_t(1), nav_map->at(download_url).size());
  VerifyNavigationEvent(initial_url,  // source_url
                        initial_url,  // source_main_frame_url
                        blank_url,    // original_request_url
                        blank_url,    // destination_url
                        false,        // is_user_initiated,
                        false,        // has_committed
                        false,        // has_server_redirect
                        nav_map->at(blank_url).at(0));
  // Source and target are at different tabs.
  EXPECT_NE(nav_map->at(blank_url).at(0).source_tab_id,
            nav_map->at(blank_url).at(0).target_tab_id);
  VerifyNavigationEvent(GURL(),     // source_url
                        GURL(),     // source_main_frame_url
                        blank_url,  // original_request_url
                        blank_url,  // destination_url
                        false,      // is_user_initiated,
                        false,      // has_committed
                        false,      // has_server_redirect
                        nav_map->at(blank_url).at(1));
  EXPECT_EQ(nav_map->at(blank_url).at(1).source_tab_id,
            nav_map->at(blank_url).at(1).target_tab_id);
  VerifyNavigationEvent(blank_url,     // source_url
                        blank_url,     // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_map->at(download_url).at(0));
  EXPECT_EQ(nav_map->at(download_url).at(0).source_tab_id,
            nav_map->at(download_url).at(0).target_tab_id);
  VerifyHostToIpMap();
}

// Use javascript to open download in a new tab and download has a data url.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       NewTabDownloadWithDataURL) {
  ClickTestLink("new_tab_download_with_data_url", 2);
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL download_url = GURL(kDownloadDataURL);
  GURL blank_url = GURL("about:blank");
  auto nav_map = navigation_map();
  ASSERT_TRUE(nav_map);
  ASSERT_EQ(std::size_t(2), nav_map->size());
  ASSERT_EQ(std::size_t(2), nav_map->at(blank_url).size());
  ASSERT_EQ(std::size_t(1), nav_map->at(download_url).size());
  // The first one comes from NOTIFICATION_RETARGETING.
  VerifyNavigationEvent(initial_url,  // source_url
                        initial_url,  // source_main_frame_url
                        blank_url,    // original_request_url
                        blank_url,    // destination_url
                        false,        // is_user_initiated,
                        false,        // has_committed
                        false,        // has_server_redirect
                        nav_map->at(blank_url).at(0));
  // Source and target are at different tabs.
  EXPECT_FALSE(nav_map->at(blank_url).at(0).source_tab_id ==
               nav_map->at(blank_url).at(0).target_tab_id);
  VerifyNavigationEvent(GURL(),     // source_url
                        GURL(),     // source_main_frame_url
                        blank_url,  // original_request_url
                        blank_url,  // destination_url
                        false,      // is_user_initiated,
                        false,      // has_committed
                        false,      // has_server_redirect
                        nav_map->at(blank_url).at(1));
  EXPECT_EQ(nav_map->at(blank_url).at(1).source_tab_id,
            nav_map->at(blank_url).at(1).target_tab_id);
  VerifyNavigationEvent(blank_url,     // source_url
                        blank_url,     // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_map->at(download_url).at(0));
  EXPECT_TRUE(nav_map->at(download_url).at(0).source_tab_id ==
              nav_map->at(download_url).at(0).target_tab_id);
  // Since data url does does not have IP, host_to_ip_map_ should be empty.
  EXPECT_EQ(std::size_t(0), host_to_ip_map()->size());
}

// TODO(jialiul): Need to figure out why this test is failing on Windows and
// flaky on other platforms.
#define MAYBE_DownloadViaHTML5FileApi DISABLED_DownloadViaHTML5FileApi
// Download via html5 file API.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       MAYBE_DownloadViaHTML5FileApi) {
  ClickTestLink("html5_file_api", 1);
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  std::string download_url_str =
      base::StringPrintf("filesystem:%stemporary/test.exe",
                         embedded_test_server()->base_url().spec().c_str());
  GURL download_url = GURL(download_url_str);
  auto nav_map = navigation_map();
  ASSERT_TRUE(nav_map);
  ASSERT_EQ(std::size_t(1), nav_map->size());
  ASSERT_EQ(std::size_t(1), nav_map->at(download_url).size());
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_map->at(download_url).at(0));
  VerifyHostToIpMap();
}

// Click a link in a subframe and start download.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       SubFrameDirectDownload) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kMultiFrameTestURL));
  std::string test_name =
      base::StringPrintf("%s', '%s", "iframe1", "iframe_direct_download");
  ClickTestLink(test_name.c_str(), 1);
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL multi_frame_test_url =
      embedded_test_server()->GetURL(kMultiFrameTestURL);
  GURL iframe_url = embedded_test_server()->GetURL(kIframeDirectDownloadURL);
  GURL iframe_retargeting_url =
      embedded_test_server()->GetURL(kIframeRetargetingURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  auto nav_map = navigation_map();
  ASSERT_TRUE(nav_map);
  ASSERT_EQ(std::size_t(4), nav_map->size());
  ASSERT_EQ(std::size_t(1), nav_map->at(multi_frame_test_url).size());
  ASSERT_EQ(std::size_t(1), nav_map->at(iframe_url).size());
  ASSERT_EQ(std::size_t(1), nav_map->at(iframe_retargeting_url).size());
  ASSERT_EQ(std::size_t(1), nav_map->at(download_url).size());
  VerifyNavigationEvent(initial_url,           // source_url
                        initial_url,           // source_main_frame_url
                        multi_frame_test_url,  // original_request_url
                        multi_frame_test_url,  // destination_url
                        true,                  // is_user_initiated,
                        true,                  // has_committed
                        false,                 // has_server_redirect
                        nav_map->at(multi_frame_test_url).at(0));
  VerifyNavigationEvent(GURL(),                // source_url
                        multi_frame_test_url,  // source_main_frame_url
                        iframe_url,            // original_request_url
                        iframe_url,            // destination_url
                        false,                 // is_user_initiated,
                        true,                  // has_committed
                        false,                 // has_server_redirect
                        nav_map->at(iframe_url).at(0));
  VerifyNavigationEvent(GURL(),                  // source_url
                        multi_frame_test_url,    // source_main_frame_url
                        iframe_retargeting_url,  // original_request_url
                        iframe_retargeting_url,  // destination_url
                        false,                   // is_user_initiated,
                        true,                    // has_committed
                        false,                   // has_server_redirect
                        nav_map->at(iframe_retargeting_url).at(0));
  VerifyNavigationEvent(iframe_url,            // source_url
                        multi_frame_test_url,  // source_main_frame_url
                        download_url,          // original_request_url
                        download_url,          // destination_url
                        false,                 // is_user_initiated,
                        false,                 // has_committed
                        false,                 // has_server_redirect
                        nav_map->at(download_url).at(0));
  VerifyHostToIpMap();
}

// Click a link in a subframe and open download in a new tab.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest,
                       SubFrameNewTabDownload) {
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(kMultiFrameTestURL));
  std::string test_name =
      base::StringPrintf("%s', '%s", "iframe2", "iframe_new_tab_download");
  ClickTestLink(test_name.c_str(), 2);
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL multi_frame_test_url =
      embedded_test_server()->GetURL(kMultiFrameTestURL);
  GURL iframe_url = embedded_test_server()->GetURL(kIframeDirectDownloadURL);
  GURL iframe_retargeting_url =
      embedded_test_server()->GetURL(kIframeRetargetingURL);
  GURL blank_url = GURL(url::kAboutBlankURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  auto nav_map = navigation_map();
  ASSERT_TRUE(nav_map);
  ASSERT_EQ(std::size_t(5), nav_map->size());
  ASSERT_EQ(std::size_t(1), nav_map->at(multi_frame_test_url).size());
  ASSERT_EQ(std::size_t(1), nav_map->at(iframe_url).size());
  ASSERT_EQ(std::size_t(1), nav_map->at(iframe_retargeting_url).size());
  ASSERT_EQ(std::size_t(2), nav_map->at(blank_url).size());
  ASSERT_EQ(std::size_t(1), nav_map->at(download_url).size());
  VerifyNavigationEvent(initial_url,           // source_url
                        initial_url,           // source_main_frame_url
                        multi_frame_test_url,  // original_request_url
                        multi_frame_test_url,  // destination_url
                        true,                  // is_user_initiated,
                        true,                  // has_committed
                        false,                 // has_server_redirect
                        nav_map->at(multi_frame_test_url).at(0));
  VerifyNavigationEvent(GURL(),                // source_url
                        multi_frame_test_url,  // source_main_frame_url
                        iframe_url,            // original_request_url
                        iframe_url,            // destination_url
                        false,                 // is_user_initiated,
                        true,                  // has_committed
                        false,                 // has_server_redirect
                        nav_map->at(iframe_url).at(0));
  VerifyNavigationEvent(GURL(),                  // source_url
                        multi_frame_test_url,    // source_main_frame_url
                        iframe_retargeting_url,  // original_request_url
                        iframe_retargeting_url,  // destination_url
                        false,                   // is_user_initiated,
                        true,                    // has_committed
                        false,                   // has_server_redirect
                        nav_map->at(iframe_retargeting_url).at(0));
  VerifyNavigationEvent(iframe_retargeting_url,  // source_url
                        multi_frame_test_url,    // source_main_frame_url
                        blank_url,               // original_request_url
                        blank_url,               // destination_url
                        false,                   // is_user_initiated,
                        false,                   // has_committed
                        false,                   // has_server_redirect
                        nav_map->at(blank_url).at(0));
  VerifyNavigationEvent(GURL(),     // source_url
                        GURL(),     // source_main_frame_url
                        blank_url,  // original_request_url
                        blank_url,  // destination_url
                        false,      // is_user_initiated,
                        false,      // has_committed
                        false,      // has_server_redirect
                        nav_map->at(blank_url).at(1));
  VerifyNavigationEvent(blank_url,     // source_url
                        blank_url,     // source_main_frame_url
                        download_url,  // original_request_url
                        download_url,  // destination_url
                        false,         // is_user_initiated,
                        false,         // has_committed
                        false,         // has_server_redirect
                        nav_map->at(download_url).at(0));
  VerifyHostToIpMap();
}

// Server-side redirect.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest, ServerRedirect) {
  GURL initial_url = embedded_test_server()->GetURL(kSingleFrameTestURL);
  GURL download_url = embedded_test_server()->GetURL(kDownloadItemURL);
  GURL request_url =
      embedded_test_server()->GetURL("/server-redirect?" + download_url.spec());
  ui_test_utils::NavigateToURL(browser(), request_url);
  CancelDownloads();
  auto nav_map = navigation_map();
  ASSERT_TRUE(nav_map);
  ASSERT_EQ(std::size_t(1), nav_map->size());
  ASSERT_EQ(std::size_t(1), nav_map->at(download_url).size());
  VerifyNavigationEvent(initial_url,   // source_url
                        initial_url,   // source_main_frame_url
                        request_url,   // original_request_url
                        download_url,  // destination_url
                        true,          // is_user_initiated,
                        false,         // has_committed
                        true,          // has_server_redirect
                        nav_map->at(download_url).at(0));
}

// host_to_ip_map_ size should increase by one after a new navigation.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest, AddIPMapping) {
  auto ip_map = host_to_ip_map();
  std::string test_server_host(embedded_test_server()->base_url().host());
  ip_map->insert(
      std::make_pair(test_server_host, std::vector<ResolvedIPAddress>()));
  ASSERT_EQ(std::size_t(0), ip_map->at(test_server_host).size());
  ClickTestLink("direct_download", 1);
  EXPECT_EQ(std::size_t(1), ip_map->at(test_server_host).size());
  EXPECT_EQ(embedded_test_server()->host_port_pair().host(),
            ip_map->at(test_server_host).back().ip);
}

// If we have already seen an IP associated with a host, update its timestamp.
IN_PROC_BROWSER_TEST_F(SBNavigationObserverBrowserTest, IPListDedup) {
  auto ip_map = host_to_ip_map();
  std::string test_server_host(embedded_test_server()->base_url().host());
  ip_map->insert(
      std::make_pair(test_server_host, std::vector<ResolvedIPAddress>()));
  base::Time yesterday(base::Time::Now() - base::TimeDelta::FromDays(1));
  ip_map->at(test_server_host)
      .push_back(ResolvedIPAddress(
          yesterday, embedded_test_server()->host_port_pair().host()));
  ASSERT_EQ(std::size_t(1), ip_map->at(test_server_host).size());
  ClickTestLink("direct_download", 1);
  EXPECT_EQ(std::size_t(1), ip_map->at(test_server_host).size());
  EXPECT_EQ(embedded_test_server()->host_port_pair().host(),
            ip_map->at(test_server_host).back().ip);
  EXPECT_NE(yesterday, ip_map->at(test_server_host).front().timestamp);
}

}  // namespace safe_browsing
