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
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/notification_types.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"

using namespace net::test_server;

namespace extensions {

enum class LinkTarget {
  SELF,
  BLANK,
};

namespace {

// Creates an <a> element, sets its href and target to |href| and |target|
// respectively, adds it to the DOM, and clicks on it. Returns once the link
// has loaded.
void ClickLinkAndWait(content::WebContents* web_contents,
                      const GURL& target_url,
                      LinkTarget target) {
  ui_test_utils::UrlLoadObserver url_observer(
      target_url, content::NotificationService::AllSources());
  std::string script = base::StringPrintf(
      "const link = document.createElement('a');"
      "link.href = '%s';"
      "link.target = '%s';"
      "document.body.appendChild(link);"
      "const event = new MouseEvent('click', {'view': window});"
      "link.dispatchEvent(event);",
      target_url.spec().c_str(),
      target == LinkTarget::SELF ? "_self" : "_blank");
  EXPECT_TRUE(content::ExecuteScript(web_contents, script));
  url_observer.Wait();
}

// Uses |params| to navigate to a URL. Blocks until the URL is loaded.
void NavigateToURLAndWait(chrome::NavigateParams* params) {
  ui_test_utils::UrlLoadObserver url_observer(
      params->url, content::NotificationService::AllSources());
  ui_test_utils::NavigateToURL(params);
  url_observer.Wait();
}

}  // namespace

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

  // Checks that, after running |action|, the initial tab's window doesn't have
  // any new tabs, the initial tab did not navigate, and that a new app window
  // with |target_url| is opened.
  void TestTabActionOpensAppWindow(const GURL& target_url,
                                   const base::Closure& action) {
    content::WebContents* initial_tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    GURL initial_url = initial_tab->GetLastCommittedURL();
    int num_tabs = browser()->tab_strip_model()->count();
    size_t num_browsers = chrome::GetBrowserCount(profile());

    action.Run();

    EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
    EXPECT_EQ(++num_browsers, chrome::GetBrowserCount(profile()));
    EXPECT_NE(browser(), chrome::FindLastActive());

    EXPECT_EQ(initial_url, initial_tab->GetLastCommittedURL());
    EXPECT_EQ(target_url, chrome::FindLastActive()
                              ->tab_strip_model()
                              ->GetActiveWebContents()
                              ->GetLastCommittedURL());
  }

  // Checks that no new windows are opened after running |action| and that the
  // existing window is still the active one and navigated to |target_url|.
  void TestTabActionDoesNotOpenAppWindow(const GURL& target_url,
                                         const base::Closure& action) {
    content::WebContents* initial_tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    int num_tabs = browser()->tab_strip_model()->count();
    size_t num_browsers = chrome::GetBrowserCount(profile());

    action.Run();

    EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
    EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
    EXPECT_EQ(browser(), chrome::FindLastActive());
    EXPECT_EQ(initial_tab,
              browser()->tab_strip_model()->GetActiveWebContents());
    EXPECT_EQ(target_url, initial_tab->GetLastCommittedURL());
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

  const GURL app_url = embedded_test_server()->GetURL(kAppUrlPath);
  TestTabActionDoesNotOpenAppWindow(
      app_url, base::Bind(&ClickLinkAndWait,
                          browser()->tab_strip_model()->GetActiveWebContents(),
                          app_url, LinkTarget::SELF));
}

// Tests that navigating to the Web App's app_url doesn't open a new window
// if features::kDesktopPWAWindowing is disabled after installing the app.
IN_PROC_BROWSER_TEST_F(BookmarkAppUrlRedirectorBrowserTest,
                       FeatureDisable_AfterInstall) {
  InstallTestBookmarkApp();
  ResetFeatureList();
  NavigateToLaunchingPage();

  const GURL app_url = embedded_test_server()->GetURL(kAppUrlPath);
  TestTabActionDoesNotOpenAppWindow(
      app_url, base::Bind(&ClickLinkAndWait,
                          browser()->tab_strip_model()->GetActiveWebContents(),
                          app_url, LinkTarget::SELF));
}

// Tests that most transition types for navigations to in-scope or
// out-of-scope URLs do not result in new app windows.
class BookmarkAppUrlRedirectorNavigationBrowserTest
    : public BookmarkAppUrlRedirectorBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<std::string, ui::PageTransition>> {};

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    BookmarkAppUrlRedirectorNavigationBrowserTest,
    testing::Combine(testing::Values(kInScopeUrlPath, kOutOfScopeUrlPath),
                     testing::Range(ui::PAGE_TRANSITION_FIRST,
                                    ui::PAGE_TRANSITION_LAST_CORE)));

IN_PROC_BROWSER_TEST_P(BookmarkAppUrlRedirectorNavigationBrowserTest,
                       MainFrameNavigations) {
  InstallTestBookmarkApp();

  GURL target_url = embedded_test_server()->GetURL(std::get<0>(GetParam()));
  ui::PageTransition transition = std::get<1>(GetParam());
  chrome::NavigateParams params(browser(), target_url, transition);

  if (!ui::PageTransitionIsMainFrame(transition)) {
    // Subframe navigations require a different setup. See
    // BookmarkAppUrlRedirectorBrowserTest.SubframeNavigation.
    return;
  }

  if (ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_LINK, transition) &&
      target_url == embedded_test_server()->GetURL(kInScopeUrlPath)) {
    TestTabActionOpensAppWindow(target_url,
                                base::Bind(&NavigateToURLAndWait, &params));
  } else {
    TestTabActionDoesNotOpenAppWindow(
        target_url, base::Bind(&NavigateToURLAndWait, &params));
  }
}

// Tests that navigations in subframes don't open new app windows.
//
// The transition type for subframe navigations is not set until
// after NavigationThrottles run. Because of this, our AppUrlRedirector
// NavigationThrottle will not see the transition type as
// PAGE_TRANSITION_AUTO_SUBFRAME/PAGE_TRANSITION_MANUAL_SUBFRAME, even though,
// by the end of the navigation, the transition type is
// PAGE_TRANSITION_AUTO_SUBFRAME/PAGE_TRANSITON_MANUAL_SUBFRAME.
IN_PROC_BROWSER_TEST_F(BookmarkAppUrlRedirectorBrowserTest,
                       SubframeNavigation) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  size_t num_browsers = chrome::GetBrowserCount(profile());
  int num_tabs = browser()->tab_strip_model()->count();
  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  ASSERT_TRUE(
      content::ExecuteScript(initial_tab,
                             "let iframe = document.createElement('iframe');"
                             "document.body.appendChild(iframe);"));

  auto all_frames = initial_tab->GetAllFrames();
  content::RenderFrameHost* main_frame = initial_tab->GetMainFrame();
  ASSERT_EQ(2u, all_frames.size());
  auto it = std::find_if(all_frames.begin(), all_frames.end(),
                         [main_frame](content::RenderFrameHost* frame) {
                           return main_frame != frame;
                         });
  ASSERT_NE(all_frames.end(), it);
  content::RenderFrameHost* iframe = *it;

  {
    content::TestFrameNavigationObserver observer(iframe);
    const GURL app_url = embedded_test_server()->GetURL(kAppUrlPath);
    chrome::NavigateParams params(browser(), app_url, ui::PAGE_TRANSITION_LINK);
    params.frame_tree_node_id = iframe->GetFrameTreeNodeId();
    ui_test_utils::NavigateToURL(&params);
    ASSERT_TRUE(ui::PageTransitionCoreTypeIs(
        observer.transition_type(), ui::PAGE_TRANSITION_AUTO_SUBFRAME));

    EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
    EXPECT_EQ(browser(), chrome::FindLastActive());
    EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
    EXPECT_EQ(app_url, iframe->GetLastCommittedURL());
  }

  {
    content::TestFrameNavigationObserver observer(iframe);
    const GURL in_scope_url = embedded_test_server()->GetURL(kInScopeUrlPath);
    chrome::NavigateParams params(browser(), in_scope_url,
                                  ui::PAGE_TRANSITION_LINK);
    params.frame_tree_node_id = iframe->GetFrameTreeNodeId();
    ui_test_utils::NavigateToURL(&params);
    ASSERT_TRUE(ui::PageTransitionCoreTypeIs(
        observer.transition_type(), ui::PAGE_TRANSITION_MANUAL_SUBFRAME));

    EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
    EXPECT_EQ(browser(), chrome::FindLastActive());
    EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
    EXPECT_EQ(in_scope_url, iframe->GetLastCommittedURL());
  }
}

// Tests that clicking a link with target="_self" to the app's app_url opens the
// Bookmark App.
IN_PROC_BROWSER_TEST_F(BookmarkAppUrlRedirectorBrowserTest, AppUrlSelf) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL app_url = embedded_test_server()->GetURL(kAppUrlPath);
  TestTabActionOpensAppWindow(
      app_url, base::Bind(&ClickLinkAndWait,
                          browser()->tab_strip_model()->GetActiveWebContents(),
                          app_url, LinkTarget::SELF));
}

// Tests that clicking a link with target="_self" to a URL in the Web App's
// scope opens a new browser window.
IN_PROC_BROWSER_TEST_F(BookmarkAppUrlRedirectorBrowserTest, InScopeUrlSelf) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL in_scope_url = embedded_test_server()->GetURL(kInScopeUrlPath);
  TestTabActionOpensAppWindow(
      in_scope_url,
      base::Bind(&ClickLinkAndWait,
                 browser()->tab_strip_model()->GetActiveWebContents(),
                 in_scope_url, LinkTarget::SELF));
}

// Tests that clicking a link with target="_self" to a URL out of the Web App's
// scope but with the same origin doesn't open a new browser window.
IN_PROC_BROWSER_TEST_F(BookmarkAppUrlRedirectorBrowserTest, OutOfScopeUrlSelf) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL out_of_scope_url =
      embedded_test_server()->GetURL(kOutOfScopeUrlPath);
  TestTabActionDoesNotOpenAppWindow(
      out_of_scope_url,
      base::Bind(&ClickLinkAndWait,
                 browser()->tab_strip_model()->GetActiveWebContents(),
                 out_of_scope_url, LinkTarget::SELF));
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
    const GURL in_scope_url = embedded_test_server()->GetURL(kInScopeUrlPath);
    ClickLinkAndWait(app_web_contents, in_scope_url, LinkTarget::SELF);

    EXPECT_EQ(num_tabs_browser, browser()->tab_strip_model()->count());
    EXPECT_EQ(num_tabs_app_browser, app_browser->tab_strip_model()->count());
    EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
    EXPECT_EQ(app_browser, chrome::FindLastActive());

    EXPECT_EQ(in_scope_url, app_web_contents->GetLastCommittedURL());
  }

  {
    const GURL out_of_scope_url =
        embedded_test_server()->GetURL(kOutOfScopeUrlPath);
    ClickLinkAndWait(app_web_contents, out_of_scope_url, LinkTarget::SELF);

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
