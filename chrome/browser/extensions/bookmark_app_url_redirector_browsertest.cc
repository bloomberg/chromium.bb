// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/bookmark_app_helper.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/notification_types.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {

const char kAppUrlPath[] =
    "/extensions/bookmark_apps/url_handlers/in_scope/index.html";
const char kScopePath[] = "/extensions/bookmark_apps/url_handlers/in_scope/";
const char kInScopeUrlPath[] =
    "/extensions/bookmark_apps/url_handlers/in_scope/other.html";
const char kOutOfScopeUrlPath[] =
    "/extensions/bookmark_apps/url_handlers/out_of_scope/other.html";

class BookmarkAppUrlRedirectorBrowserTest : public ExtensionBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_ = base::MakeUnique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitAndEnableFeature(features::kDesktopPWAWindowing);
    ExtensionBrowserTest::SetUp();
  }

  void InstallTestBookmarkApp() {
    ASSERT_TRUE(embedded_test_server()->Start());
    size_t num_extensions =
        ExtensionRegistry::Get(profile())->enabled_extensions().size();

    WebApplicationInfo web_app_info;
    web_app_info.app_url = embedded_test_server()->GetURL(kAppUrlPath);
    web_app_info.scope = embedded_test_server()->GetURL(kScopePath);
    web_app_info.title = base::UTF8ToUTF16("Test app");
    web_app_info.description = base::UTF8ToUTF16("Test description");

    content::WindowedNotificationObserver windowed_observer(
        extensions::NOTIFICATION_CRX_INSTALLER_DONE,
        content::NotificationService::AllSources());
    extensions::CreateOrUpdateBookmarkApp(extension_service(), &web_app_info);
    windowed_observer.Wait();

    ASSERT_EQ(++num_extensions,
              ExtensionRegistry::Get(profile())->enabled_extensions().size());
  }

  Browser* OpenTestBookmarkApp() {
    GURL app_url = embedded_test_server()->GetURL(kAppUrlPath);
    ui_test_utils::UrlLoadObserver url_observer(
        app_url, content::NotificationService::AllSources());
    ui_test_utils::NavigateToURL(browser(), app_url);
    url_observer.Wait();

    return chrome::FindLastActive();
  }

  void ResetFeatureList() { scoped_feature_list_.reset(); }

 private:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

// Tests that navigating to the Web App's app_url doesn't open a new window
// if features::kDesktopPWAWindowing is disabled before installing the app.
IN_PROC_BROWSER_TEST_F(BookmarkAppUrlRedirectorBrowserTest,
                       FeatureDisable_BeforeInstall) {
  ResetFeatureList();
  InstallTestBookmarkApp();

  int num_tabs = browser()->tab_strip_model()->count();
  size_t num_browsers = chrome::GetBrowserCount(profile());

  GURL app_url = embedded_test_server()->GetURL(kAppUrlPath);
  ui_test_utils::UrlLoadObserver url_observer(
      app_url, content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(browser(), app_url);
  url_observer.Wait();

  EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
  EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
  EXPECT_EQ(browser(), chrome::FindLastActive());

  EXPECT_EQ(app_url, browser()
                         ->tab_strip_model()
                         ->GetActiveWebContents()
                         ->GetLastCommittedURL());
}

// Tests that navigating to the Web App's app_url doesn't open a new window
// if features::kDesktopPWAWindowing is disabled after installing the app.
IN_PROC_BROWSER_TEST_F(BookmarkAppUrlRedirectorBrowserTest,
                       FeatureDisable_AfterInstall) {
  InstallTestBookmarkApp();
  ResetFeatureList();

  int num_tabs = browser()->tab_strip_model()->count();
  size_t num_browsers = chrome::GetBrowserCount(profile());

  GURL app_url = embedded_test_server()->GetURL(kAppUrlPath);
  ui_test_utils::UrlLoadObserver url_observer(
      app_url, content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(browser(), app_url);
  url_observer.Wait();

  EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
  EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
  EXPECT_EQ(browser(), chrome::FindLastActive());

  EXPECT_EQ(app_url, browser()
                         ->tab_strip_model()
                         ->GetActiveWebContents()
                         ->GetLastCommittedURL());
}

// Tests that navigating to the Web App's app_url opens a new browser window.
IN_PROC_BROWSER_TEST_F(BookmarkAppUrlRedirectorBrowserTest, AppUrl) {
  InstallTestBookmarkApp();

  int num_tabs = browser()->tab_strip_model()->count();
  size_t num_browsers = chrome::GetBrowserCount(profile());
  GURL initial_url = browser()
                         ->tab_strip_model()
                         ->GetActiveWebContents()
                         ->GetLastCommittedURL();

  GURL app_url = embedded_test_server()->GetURL(kAppUrlPath);
  ui_test_utils::UrlLoadObserver url_observer(
      app_url, content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(browser(), app_url);
  url_observer.Wait();

  EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
  EXPECT_EQ(++num_browsers, chrome::GetBrowserCount(profile()));
  EXPECT_NE(browser(), chrome::FindLastActive());

  EXPECT_EQ(initial_url, browser()
                             ->tab_strip_model()
                             ->GetActiveWebContents()
                             ->GetLastCommittedURL());
  EXPECT_EQ(app_url, chrome::FindLastActive()
                         ->tab_strip_model()
                         ->GetActiveWebContents()
                         ->GetLastCommittedURL());
}

// Tests that navigating to a URL in the Web App's scope opens a new browser
// window.
IN_PROC_BROWSER_TEST_F(BookmarkAppUrlRedirectorBrowserTest, InScopeUrl) {
  InstallTestBookmarkApp();

  int num_tabs = browser()->tab_strip_model()->count();
  size_t num_browsers = chrome::GetBrowserCount(profile());
  GURL initial_url = browser()
                         ->tab_strip_model()
                         ->GetActiveWebContents()
                         ->GetLastCommittedURL();

  GURL in_scope_url = embedded_test_server()->GetURL(kInScopeUrlPath);
  ui_test_utils::UrlLoadObserver url_observer(
      in_scope_url, content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(browser(), in_scope_url);
  url_observer.Wait();

  EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
  EXPECT_EQ(++num_browsers, chrome::GetBrowserCount(profile()));
  EXPECT_NE(browser(), chrome::FindLastActive());

  EXPECT_EQ(initial_url, browser()
                             ->tab_strip_model()
                             ->GetActiveWebContents()
                             ->GetLastCommittedURL());
  EXPECT_EQ(in_scope_url, chrome::FindLastActive()
                              ->tab_strip_model()
                              ->GetActiveWebContents()
                              ->GetLastCommittedURL());
}

// Tests that navigating to a URL out of the Web App's scope but with the
// same origin doesn't open a new browser window.
IN_PROC_BROWSER_TEST_F(BookmarkAppUrlRedirectorBrowserTest, OutOfScopeUrl) {
  InstallTestBookmarkApp();

  int num_tabs = browser()->tab_strip_model()->count();
  size_t num_browsers = chrome::GetBrowserCount(profile());

  GURL out_of_scope_url = embedded_test_server()->GetURL(kOutOfScopeUrlPath);
  ui_test_utils::UrlLoadObserver url_observer(
      out_of_scope_url, content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(browser(), out_of_scope_url);
  url_observer.Wait();

  EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
  EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
  EXPECT_EQ(browser(), chrome::FindLastActive());

  EXPECT_EQ(out_of_scope_url, chrome::FindLastActive()
                                  ->tab_strip_model()
                                  ->GetActiveWebContents()
                                  ->GetLastCommittedURL());
}

// Tests that navigating inside the app doesn't open new browser windows.
IN_PROC_BROWSER_TEST_F(BookmarkAppUrlRedirectorBrowserTest, InAppNavigation) {
  InstallTestBookmarkApp();
  Browser* app_browser = OpenTestBookmarkApp();

  int num_tabs_browser = browser()->tab_strip_model()->count();
  int num_tabs_app_browser = app_browser->tab_strip_model()->count();
  size_t num_browsers = chrome::GetBrowserCount(profile());

  {
    GURL in_scope_url = embedded_test_server()->GetURL(kInScopeUrlPath);
    ui_test_utils::UrlLoadObserver url_observer(
        in_scope_url, content::NotificationService::AllSources());
    ui_test_utils::NavigateToURL(app_browser, in_scope_url);
    url_observer.Wait();

    EXPECT_EQ(num_tabs_browser, browser()->tab_strip_model()->count());
    EXPECT_EQ(num_tabs_app_browser, app_browser->tab_strip_model()->count());
    EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
    EXPECT_EQ(app_browser, chrome::FindLastActive());

    EXPECT_EQ(in_scope_url, app_browser->tab_strip_model()
                                ->GetActiveWebContents()
                                ->GetLastCommittedURL());
  }

  {
    GURL out_of_scope_url = embedded_test_server()->GetURL(kOutOfScopeUrlPath);
    ui_test_utils::UrlLoadObserver url_observer(
        out_of_scope_url, content::NotificationService::AllSources());
    ui_test_utils::NavigateToURL(app_browser, out_of_scope_url);
    url_observer.Wait();

    EXPECT_EQ(num_tabs_browser, browser()->tab_strip_model()->count());
    EXPECT_EQ(num_tabs_app_browser, app_browser->tab_strip_model()->count());
    EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
    EXPECT_EQ(app_browser, chrome::FindLastActive());

    EXPECT_EQ(out_of_scope_url, app_browser->tab_strip_model()
                                    ->GetActiveWebContents()
                                    ->GetLastCommittedURL());
  }
}

}  // namespace extensions
