// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_BROWSERTEST_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_BROWSERTEST_H_

#include "base/command_line.h"
#include "chrome/browser/signin/chrome_signin_client.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/signin/login_ui_service.h"
#include "chrome/browser/ui/webui/signin/login_ui_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"

namespace {
const char kNonSigninURL[] = "http://www.google.com";
}

class SigninBrowserTest : public InProcessBrowserTest {
 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    https_server_.reset(new net::SpawnedTestServer(
        net::SpawnedTestServer::TYPE_HTTPS,
        net::SpawnedTestServer::kLocalhost,
        base::FilePath(FILE_PATH_LITERAL("chrome/test/data"))));
    ASSERT_TRUE(https_server_->Start());

    // Add a host resolver rule to map all outgoing requests to the test server.
    // This allows us to use "real" hostnames in URLs, which we can use to
    // create arbitrary SiteInstances.
    command_line->AppendSwitchASCII(
        switches::kHostResolverRules,
        "MAP * " + https_server_->host_port_pair().ToString() +
            ",EXCLUDE localhost");
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
    // All tests in this file are for the web based sign in flows.
    // TODO(guohui): fix tests for inline sign in flows.
    command_line->AppendSwitch(switches::kEnableWebBasedSignin);
  }

  virtual void SetUp() OVERRIDE {
    factory_.reset(new net::URLFetcherImplFactory());
    fake_factory_.reset(new net::FakeURLFetcherFactory(factory_.get()));
    fake_factory_->SetFakeResponse(
        GaiaUrls::GetInstance()->service_login_url(), std::string(),
        net::HTTP_OK, net::URLRequestStatus::SUCCESS);
    fake_factory_->SetFakeResponse(
        GURL(kNonSigninURL), std::string(), net::HTTP_OK,
        net::URLRequestStatus::SUCCESS);
    // Yield control back to the InProcessBrowserTest framework.
    InProcessBrowserTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    if (fake_factory_.get()) {
      fake_factory_->ClearFakeResponses();
      fake_factory_.reset();
    }

    // Cancel any outstanding URL fetches and destroy the URLFetcherImplFactory
    // we created.
    net::URLFetcher::CancelAll();
    factory_.reset();
    InProcessBrowserTest::TearDown();
  }

 private:
  // Fake URLFetcher factory used to mock out GAIA signin.
  scoped_ptr<net::FakeURLFetcherFactory> fake_factory_;

  // The URLFetcherImplFactory instance used to instantiate |fake_factory_|.
  scoped_ptr<net::URLFetcherImplFactory> factory_;

  scoped_ptr<net::SpawnedTestServer> https_server_;
};

// If the one-click-signin feature is not enabled (e.g Chrome OS), we
// never grant signin privileges to any renderer processes.
#if defined(ENABLE_ONE_CLICK_SIGNIN)
const bool kOneClickSigninEnabled = true;
#else
const bool kOneClickSigninEnabled = false;
#endif

// Disabled on Windows due to flakiness. http://crbug.com/249055
#if defined(OS_WIN)
#define MAYBE_ProcessIsolation DISABLED_ProcessIsolation
#else
#define MAYBE_ProcessIsolation ProcessIsolation
#endif
IN_PROC_BROWSER_TEST_F(SigninBrowserTest, MAYBE_ProcessIsolation) {
  SigninClient* signin =
      ChromeSigninClientFactory::GetForProfile(browser()->profile());
  EXPECT_FALSE(signin->HasSigninProcess());

  ui_test_utils::NavigateToURL(browser(), signin::GetPromoURL(
      signin::SOURCE_NTP_LINK, true));
  EXPECT_EQ(kOneClickSigninEnabled, signin->HasSigninProcess());

  // Navigating away should change the process.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIOmniboxURL));
  EXPECT_FALSE(signin->HasSigninProcess());

  ui_test_utils::NavigateToURL(browser(), signin::GetPromoURL(
      signin::SOURCE_NTP_LINK, true));
  EXPECT_EQ(kOneClickSigninEnabled, signin->HasSigninProcess());

  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  int active_tab_process_id =
      active_tab->GetRenderProcessHost()->GetID();
  EXPECT_EQ(kOneClickSigninEnabled,
            signin->IsSigninProcess(active_tab_process_id));
  EXPECT_EQ(0, active_tab->GetRenderViewHost()->GetEnabledBindings());

  // Entry points to signin request "SINGLETON_TAB" mode, so a new request
  // shouldn't change anything.
  chrome::NavigateParams params(chrome::GetSingletonTabNavigateParams(
      browser(),
      GURL(signin::GetPromoURL(signin::SOURCE_NTP_LINK, false))));
  params.path_behavior = chrome::NavigateParams::IGNORE_AND_NAVIGATE;
  ShowSingletonTabOverwritingNTP(browser(), params);
  EXPECT_EQ(active_tab, browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(kOneClickSigninEnabled,
            signin->IsSigninProcess(active_tab_process_id));

  // Navigating away should change the process.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  EXPECT_FALSE(signin->IsSigninProcess(
      active_tab->GetRenderProcessHost()->GetID()));
}

#if defined (OS_MACOSX)
// crbug.com/375197
#define MAYBE_NotTrustedAfterRedirect DISABLED_NotTrustedAfterRedirect
#else
#define MAYBE_NotTrustedAfterRedirect NotTrustedAfterRedirect
#endif

IN_PROC_BROWSER_TEST_F(SigninBrowserTest, MAYBE_NotTrustedAfterRedirect) {
  SigninClient* signin =
      ChromeSigninClientFactory::GetForProfile(browser()->profile());
  EXPECT_FALSE(signin->HasSigninProcess());

  GURL url = signin::GetPromoURL(signin::SOURCE_NTP_LINK, true);
  ui_test_utils::NavigateToURL(browser(), url);
  EXPECT_EQ(kOneClickSigninEnabled, signin->HasSigninProcess());

  // Navigating in a different tab should not affect the sign-in process.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(kNonSigninURL), NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(kOneClickSigninEnabled, signin->HasSigninProcess());

  // Navigating away should clear the sign-in process.
  GURL redirect_url("https://accounts.google.com/server-redirect?"
      "https://foo.com?service=chromiumsync");
  ui_test_utils::NavigateToURL(browser(), redirect_url);
  EXPECT_FALSE(signin->HasSigninProcess());
}

class BackOnNTPCommitObserver : public content::WebContentsObserver {
 public:
  explicit BackOnNTPCommitObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {
  }

  virtual void DidCommitProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& url,
      ui::PageTransition transition_type) OVERRIDE {
    if (url == GURL(chrome::kChromeUINewTabURL) ||
        url == GURL(chrome::kChromeSearchLocalNtpUrl)) {
      content::WindowedNotificationObserver observer(
          content::NOTIFICATION_NAV_ENTRY_COMMITTED,
          content::NotificationService::AllSources());
      web_contents()->GetController().GoBack();
      observer.Wait();
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BackOnNTPCommitObserver);
};

// This is a test for http://crbug.com/257277. It simulates the navigations
// that occur if the user clicks on the "Skip for now" link at the signin page
// and initiates a back navigation between the point of Commit and
// DidStopLoading of the NTP.
IN_PROC_BROWSER_TEST_F(SigninBrowserTest, SigninSkipForNowAndGoBack) {
  GURL ntp_url(chrome::kChromeUINewTabURL);
  GURL start_url = signin::GetPromoURL(signin::SOURCE_START_PAGE, false);
  GURL skip_url = signin::GetLandingURL("ntp", 1);

  SigninClient* signin =
      ChromeSigninClientFactory::GetForProfile(browser()->profile());
  EXPECT_FALSE(signin->HasSigninProcess());

  ui_test_utils::NavigateToURL(browser(), start_url);
  EXPECT_EQ(kOneClickSigninEnabled, signin->HasSigninProcess());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Simulate clicking on the Skip for now link. It's important to have a
  // link transition so that OneClickSigninHelper removes the blank page
  // from the history.
  chrome::NavigateParams navigate_params(browser(),
                                         skip_url,
                                         ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&navigate_params);

  // Register an observer that will navigate back immediately on the commit of
  // the NTP. This will allow us to hit the race condition of navigating back
  // before the DidStopLoading message of NTP gets delivered. This must be
  // created after the navigation to the skip_url has finished loading,
  // otherwise this observer will navigate back, before the history cleaner
  // has had a chance to remove the navigation entry.
  BackOnNTPCommitObserver commit_observer(web_contents);

  // Since OneClickSigninHelper aborts redirect to NTP, thus we expect the
  // visible URL to be the starting URL.
  EXPECT_EQ(skip_url, web_contents->GetLastCommittedURL());
  EXPECT_EQ(start_url, web_contents->GetVisibleURL());

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  observer.Wait();
  EXPECT_EQ(start_url, web_contents->GetLastCommittedURL());
}
#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_BROWSERTEST_H_
