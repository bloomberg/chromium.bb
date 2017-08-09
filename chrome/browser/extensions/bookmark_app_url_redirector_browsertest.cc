// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/bookmark_app_helper.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/notification_types.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"

using namespace net::test_server;

namespace extensions {

const char kLaunchingPagePath[] =
    "/extensions/bookmark_apps/url_handlers/launching_pages/index.html";
const char kAppUrlPath[] =
    "/extensions/bookmark_apps/url_handlers/in_scope/index.html";
const char kScopePath[] = "/extensions/bookmark_apps/url_handlers/in_scope/";
const char kInScopeUrlPath[] =
    "/extensions/bookmark_apps/url_handlers/in_scope/other.html";
const char kOutOfScopeUrlPath[] =
    "/extensions/bookmark_apps/url_handlers/out_of_scope/index.html";

class BookmarkAppUrlRedirectorBrowserTest : public ExtensionBrowserTest {
 public:
  enum class LinkTarget {
    SELF,
    BLANK,
  };

  void SetUp() override {
    scoped_feature_list_ = base::MakeUnique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitAndEnableFeature(features::kDesktopPWAWindowing);

    // Register a request handler that will return empty pages. Tests are
    // responsible for adding elements and firing events on these empty pages.
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating([](const HttpRequest& request) {
          auto response = base::MakeUnique<BasicHttpResponse>();
          response->set_content_type("text/html");
          return static_cast<std::unique_ptr<HttpResponse>>(
              std::move(response));
        }));

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

    test_bookmark_app_ =
        content::Details<const Extension>(windowed_observer.details()).ptr();
  }

  Browser* OpenTestBookmarkApp() {
    GURL app_url = embedded_test_server()->GetURL(kAppUrlPath);
    ui_test_utils::UrlLoadObserver url_observer(
        app_url, content::NotificationService::AllSources());

    OpenApplication(AppLaunchParams(
        profile(), test_bookmark_app_, extensions::LAUNCH_CONTAINER_WINDOW,
        WindowOpenDisposition::CURRENT_TAB, SOURCE_CHROME_INTERNAL));

    url_observer.Wait();

    return chrome::FindLastActive();
  }

  // Navigates the active tab to the launching page.
  void NavigateToLaunchingPage() {
    GURL launching_page_url =
        embedded_test_server()->GetURL(kLaunchingPagePath);
    ui_test_utils::UrlLoadObserver url_observer(
        launching_page_url, content::NotificationService::AllSources());
    ui_test_utils::NavigateToURL(browser(), launching_page_url);
    url_observer.Wait();
  }

  // Creates an <a> element, sets its href and target to |href| and |target|
  // respectively, adds it to the DOM, and clicks on it. Returns once the link
  // has loaded.
  void ClickLinkAndWait(content::WebContents* web_contents,
                        const std::string& href,
                        LinkTarget target) {
    ui_test_utils::UrlLoadObserver url_observer(
        embedded_test_server()->GetURL(href),
        content::NotificationService::AllSources());
    std::string script = base::StringPrintf(
        "const link = document.createElement('a');"
        "link.href = '%s';"
        "link.target = '%s';"
        "document.body.appendChild(link);"
        "const event = new MouseEvent('click', {'view': window});"
        "link.dispatchEvent(event);",
        href.c_str(), target == LinkTarget::SELF ? "_self" : "_blank");
    EXPECT_TRUE(content::ExecuteScript(web_contents, script));
    url_observer.Wait();
  }

  void ResetFeatureList() { scoped_feature_list_.reset(); }

 private:
  const Extension* test_bookmark_app_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

// Tests that navigating to the Web App's app_url doesn't open a new window
// if features::kDesktopPWAWindowing is disabled before installing the app.
IN_PROC_BROWSER_TEST_F(BookmarkAppUrlRedirectorBrowserTest,
                       FeatureDisable_BeforeInstall) {
  ResetFeatureList();
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  int num_tabs = browser()->tab_strip_model()->count();
  size_t num_browsers = chrome::GetBrowserCount(profile());

  ClickLinkAndWait(initial_tab, kAppUrlPath, LinkTarget::SELF);

  EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
  EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
  EXPECT_EQ(browser(), chrome::FindLastActive());

  EXPECT_EQ(embedded_test_server()->GetURL(kAppUrlPath),
            initial_tab->GetLastCommittedURL());
}

// Tests that navigating to the Web App's app_url doesn't open a new window
// if features::kDesktopPWAWindowing is disabled after installing the app.
IN_PROC_BROWSER_TEST_F(BookmarkAppUrlRedirectorBrowserTest,
                       FeatureDisable_AfterInstall) {
  InstallTestBookmarkApp();
  ResetFeatureList();
  NavigateToLaunchingPage();

  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  int num_tabs = browser()->tab_strip_model()->count();
  size_t num_browsers = chrome::GetBrowserCount(profile());

  ClickLinkAndWait(initial_tab, kAppUrlPath, LinkTarget::SELF);

  EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
  EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
  EXPECT_EQ(browser(), chrome::FindLastActive());

  EXPECT_EQ(embedded_test_server()->GetURL(kAppUrlPath),
            initial_tab->GetLastCommittedURL());
}

// Tests that navigating to a in-scope URL using the omnibox *does not*
// open the Bookmark App.
IN_PROC_BROWSER_TEST_F(BookmarkAppUrlRedirectorBrowserTest,
                       OmniboxNavigationInScope) {
  InstallTestBookmarkApp();

  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  int num_tabs = browser()->tab_strip_model()->count();
  size_t num_browsers = chrome::GetBrowserCount(profile());

  GURL in_scope_url = embedded_test_server()->GetURL(kInScopeUrlPath);
  ui_test_utils::UrlLoadObserver url_observer(
      in_scope_url, content::NotificationService::AllSources());
  chrome::NavigateParams params(browser(), in_scope_url,
                                ui::PAGE_TRANSITION_TYPED);
  ui_test_utils::NavigateToURL(&params);
  url_observer.Wait();

  EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
  EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
  EXPECT_EQ(browser(), chrome::FindLastActive());

  EXPECT_EQ(in_scope_url, initial_tab->GetLastCommittedURL());
}

// Tests that navigating to an out-of-scope URL with the same origin as the
// installed Bookmark App *does not* open the Bookmark App.
IN_PROC_BROWSER_TEST_F(BookmarkAppUrlRedirectorBrowserTest,
                       OmniboxNavigationOutOfScope) {
  InstallTestBookmarkApp();

  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  int num_tabs = browser()->tab_strip_model()->count();
  size_t num_browsers = chrome::GetBrowserCount(profile());

  GURL out_of_scope_url = embedded_test_server()->GetURL(kOutOfScopeUrlPath);
  ui_test_utils::UrlLoadObserver url_observer(
      out_of_scope_url, content::NotificationService::AllSources());
  chrome::NavigateParams params(browser(), out_of_scope_url,
                                ui::PAGE_TRANSITION_TYPED);
  ui_test_utils::NavigateToURL(&params);
  url_observer.Wait();

  EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
  EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
  EXPECT_EQ(browser(), chrome::FindLastActive());

  EXPECT_EQ(out_of_scope_url, initial_tab->GetLastCommittedURL());
}

// Tests that clicking a link with target="_self" to the apps app_url opens the
// Bookmark App.
IN_PROC_BROWSER_TEST_F(BookmarkAppUrlRedirectorBrowserTest, AppUrl) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL initial_url = initial_tab->GetLastCommittedURL();
  int num_tabs = browser()->tab_strip_model()->count();
  size_t num_browsers = chrome::GetBrowserCount(profile());

  ClickLinkAndWait(initial_tab, kAppUrlPath, LinkTarget::SELF);

  EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
  EXPECT_EQ(++num_browsers, chrome::GetBrowserCount(profile()));
  EXPECT_NE(browser(), chrome::FindLastActive());

  EXPECT_EQ(initial_url, initial_tab->GetLastCommittedURL());
  EXPECT_EQ(embedded_test_server()->GetURL(kAppUrlPath),
            chrome::FindLastActive()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL());
}

// Tests that clicking a link with target="_self" to a URL in the Web App's
// scope opens a new browser window.
IN_PROC_BROWSER_TEST_F(BookmarkAppUrlRedirectorBrowserTest, InScopeUrl) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL initial_url = initial_tab->GetLastCommittedURL();
  int num_tabs = browser()->tab_strip_model()->count();
  size_t num_browsers = chrome::GetBrowserCount(profile());

  ClickLinkAndWait(initial_tab, kInScopeUrlPath, LinkTarget::SELF);

  EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
  EXPECT_EQ(++num_browsers, chrome::GetBrowserCount(profile()));
  EXPECT_NE(browser(), chrome::FindLastActive());

  EXPECT_EQ(initial_url, initial_tab->GetLastCommittedURL());
  EXPECT_EQ(embedded_test_server()->GetURL(kInScopeUrlPath),
            chrome::FindLastActive()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL());
}

// Tests that clicking a link with target="_self" to a URL out of the Web App's
// scope but with the same origin doesn't open a new browser window.
IN_PROC_BROWSER_TEST_F(BookmarkAppUrlRedirectorBrowserTest, OutOfScopeUrl) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  int num_tabs = browser()->tab_strip_model()->count();
  size_t num_browsers = chrome::GetBrowserCount(profile());

  ClickLinkAndWait(browser()->tab_strip_model()->GetActiveWebContents(),
                   kOutOfScopeUrlPath, LinkTarget::SELF);

  EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
  EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
  EXPECT_EQ(browser(), chrome::FindLastActive());

  EXPECT_EQ(embedded_test_server()->GetURL(kOutOfScopeUrlPath),
            chrome::FindLastActive()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetLastCommittedURL());
}

// Tests that clicking links inside the app doesn't open new browser windows.
IN_PROC_BROWSER_TEST_F(BookmarkAppUrlRedirectorBrowserTest, InAppNavigation) {
  InstallTestBookmarkApp();
  Browser* app_browser = OpenTestBookmarkApp();
  content::WebContents* app_web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  int num_tabs_browser = browser()->tab_strip_model()->count();
  int num_tabs_app_browser = app_browser->tab_strip_model()->count();
  size_t num_browsers = chrome::GetBrowserCount(profile());

  {
    ClickLinkAndWait(app_web_contents, kInScopeUrlPath, LinkTarget::SELF);

    EXPECT_EQ(num_tabs_browser, browser()->tab_strip_model()->count());
    EXPECT_EQ(num_tabs_app_browser, app_browser->tab_strip_model()->count());
    EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
    EXPECT_EQ(app_browser, chrome::FindLastActive());

    EXPECT_EQ(embedded_test_server()->GetURL(kInScopeUrlPath),
              app_web_contents->GetLastCommittedURL());
  }

  {
    ClickLinkAndWait(app_web_contents, kOutOfScopeUrlPath, LinkTarget::SELF);

    EXPECT_EQ(num_tabs_browser, browser()->tab_strip_model()->count());
    EXPECT_EQ(num_tabs_app_browser, app_browser->tab_strip_model()->count());
    EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
    EXPECT_EQ(app_browser, chrome::FindLastActive());

    EXPECT_EQ(embedded_test_server()->GetURL(kOutOfScopeUrlPath),
              app_browser->tab_strip_model()
                  ->GetActiveWebContents()
                  ->GetLastCommittedURL());
  }
}

}  // namespace extensions
