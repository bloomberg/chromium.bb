// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/bookmark_app_helper.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
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
#include "content/public/common/context_menu_params.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/notification_types.h"
#include "net/base/escape.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_fetcher.h"

using namespace net::test_server;

namespace extensions {

enum class LinkTarget {
  SELF,
  BLANK,
};

namespace {

std::string CreateServerRedirect(const GURL& target_url) {
  const char* const kServerRedirectBase = "/server-redirect?";
  return kServerRedirectBase +
         net::EscapeQueryParamValue(target_url.spec(), false);
}

std::string CreateClientRedirect(const GURL& target_url) {
  const char* const kClientRedirectBase = "/client-redirect?";
  return kClientRedirectBase +
         net::EscapeQueryParamValue(target_url.spec(), false);
}

std::unique_ptr<content::TestNavigationObserver> GetTestNavigationObserver(
    const GURL& target_url) {
  auto observer = std::make_unique<content::TestNavigationObserver>(target_url);
  observer->WatchExistingWebContents();
  observer->StartWatchingNewWebContents();
  return observer;
}

// Inserts an iframe in the main frame of |web_contents|.
void InsertIFrame(content::WebContents* web_contents) {
  ASSERT_TRUE(
      content::ExecuteScript(web_contents,
                             "let iframe = document.createElement('iframe');"
                             "document.body.appendChild(iframe);"));
}

// Returns a subframe in |web_contents|.
content::RenderFrameHost* GetIFrame(content::WebContents* web_contents) {
  const auto all_frames = web_contents->GetAllFrames();
  const content::RenderFrameHost* main_frame = web_contents->GetMainFrame();

  DCHECK_EQ(2u, all_frames.size());
  auto it = std::find_if(all_frames.begin(), all_frames.end(),
                         [main_frame](content::RenderFrameHost* frame) {
                           return main_frame != frame;
                         });
  DCHECK(it != all_frames.end());
  return *it;
}

// Creates an <a> element, sets its href and target to |link_url| and |target|
// respectively, adds it to the DOM, and clicks on it. Returns once |target_url|
// has loaded.
void ClickLinkAndWaitForURL(content::WebContents* web_contents,
                            const GURL& link_url,
                            const GURL& target_url,
                            LinkTarget target) {
  auto observer = GetTestNavigationObserver(target_url);
  std::string script = base::StringPrintf(
      "(() => {"
      "const link = document.createElement('a');"
      "link.href = '%s';"
      "link.target = '%s';"
      "document.body.appendChild(link);"
      "const event = new MouseEvent('click', {'view': window});"
      "link.dispatchEvent(event);"
      "})();",
      link_url.spec().c_str(), target == LinkTarget::SELF ? "_self" : "_blank");
  ASSERT_TRUE(content::ExecuteScript(web_contents, script));
  observer->WaitForNavigationFinished();
}

// Creates an <a> element, sets its href and target to |link_url| and |target|
// respectively, adds it to the DOM, and clicks on it. Returns once the link
// has loaded.
void ClickLinkAndWait(content::WebContents* web_contents,
                      const GURL& link_url,
                      LinkTarget target) {
  ClickLinkAndWaitForURL(web_contents, link_url, link_url, target);
}

// Creates a <form> element with a |target_url| action and |method| method. Adds
// the form to the DOM with a button and clicks the button. Returns once
// |target_url| has been loaded.
//
// If |method| is net::URLFetcher::RequestType::GET, |target_url| should contain
// an empty query string, since that URL will be loaded when submitting the form
// e.g. "https://www.example.com/?".
void SubmitFormAndWait(content::WebContents* web_contents,
                       const GURL& target_url,
                       net::URLFetcher::RequestType method) {
  if (method == net::URLFetcher::RequestType::GET) {
    ASSERT_TRUE(target_url.has_query());
    ASSERT_TRUE(target_url.query().empty());
  }

  auto observer = GetTestNavigationObserver(target_url);
  std::string script = base::StringPrintf(
      "(() => {"
      "const form = document.createElement('form');"
      "form.action = '%s';"
      "form.method = '%s';"
      "const button = document.createElement('input');"
      "button.type = 'submit';"
      "form.appendChild(button);"
      "document.body.appendChild(form);"
      "button.dispatchEvent(new MouseEvent('click', {'view': window}));"
      "})();",
      target_url.spec().c_str(),
      method == net::URLFetcher::RequestType::POST ? "post" : "get");
  ASSERT_TRUE(content::ExecuteScript(web_contents, script));
  observer->WaitForNavigationFinished();
}

// Uses |params| to navigate to a URL. Blocks until the URL is loaded.
void NavigateToURLAndWait(chrome::NavigateParams* params) {
  auto observer = GetTestNavigationObserver(params->url);
  ui_test_utils::NavigateToURL(params);
  observer->WaitForNavigationFinished();
}

// Wrapper so that we can use base::Bind with NavigateToURL.
void NavigateToURLWrapper(chrome::NavigateParams* params) {
  ui_test_utils::NavigateToURL(params);
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

class BookmarkAppNavigationThrottleBrowserTest : public ExtensionBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_ = base::MakeUnique<base::test::ScopedFeatureList>();
    scoped_feature_list_->InitAndEnableFeature(features::kDesktopPWAWindowing);

    // Register a request handler that will return empty pages. Tests are
    // responsible for adding elements and firing events on these empty pages.
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating([](const HttpRequest& request) {
          // Let the default request handlers handle redirections.
          if (request.GetURL().path() == "/server-redirect" ||
              request.GetURL().path() == "/client-redirect") {
            return std::unique_ptr<HttpResponse>();
          }
          auto response = base::MakeUnique<BasicHttpResponse>();
          response->set_content_type("text/html");
          response->AddCustomHeader("Access-Control-Allow-Origin", "*");
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
    auto observer = GetTestNavigationObserver(app_url);

    OpenApplication(AppLaunchParams(
        profile(), test_bookmark_app_, extensions::LAUNCH_CONTAINER_WINDOW,
        WindowOpenDisposition::CURRENT_TAB, SOURCE_CHROME_INTERNAL));

    observer->WaitForNavigationFinished();

    return chrome::FindLastActive();
  }

  // Navigates the active tab in |browser| to the launching page.
  void NavigateToLaunchingPage(Browser* browser) {
    ui_test_utils::NavigateToURL(browser, GetLaunchingPageURL());
  }

  // Navigates the active tab to the launching page.
  void NavigateToLaunchingPage() { NavigateToLaunchingPage(browser()); }

  // Checks that, after running |action|, the initial tab's window doesn't have
  // any new tabs, the initial tab did not navigate, and that no new windows
  // have been opened.
  void TestTabActionDoesNotNavigateOrOpenAppWindow(
      const base::Closure& action) {
    size_t num_browsers = chrome::GetBrowserCount(profile());
    int num_tabs = browser()->tab_strip_model()->count();
    content::WebContents* initial_tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    GURL initial_url = initial_tab->GetLastCommittedURL();

    action.Run();

    EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
    EXPECT_EQ(browser(), chrome::FindLastActive());
    EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
    EXPECT_EQ(initial_tab,
              browser()->tab_strip_model()->GetActiveWebContents());
    EXPECT_EQ(initial_url, initial_tab->GetLastCommittedURL());
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
  // existing |browser| window is still the active one and navigated to
  // |target_url|. Returns true if there were no errors.
  bool TestTabActionDoesNotOpenAppWindow(Browser* browser,
                                         const GURL& target_url,
                                         const base::Closure& action) {
    content::WebContents* initial_tab =
        browser->tab_strip_model()->GetActiveWebContents();
    int num_tabs = browser->tab_strip_model()->count();
    size_t num_browsers = chrome::GetBrowserCount(browser->profile());

    action.Run();

    EXPECT_EQ(num_tabs, browser->tab_strip_model()->count());
    EXPECT_EQ(num_browsers, chrome::GetBrowserCount(browser->profile()));
    EXPECT_EQ(browser, chrome::FindLastActive());
    EXPECT_EQ(initial_tab, browser->tab_strip_model()->GetActiveWebContents());
    EXPECT_EQ(target_url, initial_tab->GetLastCommittedURL());

    return !HasFailure();
  }

  // Checks that no new windows are opened after running |action| and that the
  // main browser window is still the active one and navigated to |target_url|.
  // Returns true if there were no errors.
  bool TestTabActionDoesNotOpenAppWindow(const GURL& target_url,
                                         const base::Closure& action) {
    return TestTabActionDoesNotOpenAppWindow(browser(), target_url, action);
  }

  // Checks that no new windows are opened after running |action| and that the
  // iframe in the initial tab navigated to |target_url|. Returns true if there
  // were no errors.
  bool TestIFrameActionDoesNotOpenAppWindow(const GURL& target_url,
                                            const base::Closure& action) {
    size_t num_browsers = chrome::GetBrowserCount(profile());
    int num_tabs = browser()->tab_strip_model()->count();
    content::WebContents* initial_tab =
        browser()->tab_strip_model()->GetActiveWebContents();

    action.Run();

    EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
    EXPECT_EQ(browser(), chrome::FindLastActive());
    EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());

    // When Site Isolation is enabled, navigating the iframe to a different
    // origin causes the original iframe's RenderFrameHost to be deleted.
    // So we retrieve the iframe's RenderFrameHost again.
    EXPECT_EQ(target_url, GetIFrame(initial_tab)->GetLastCommittedURL());

    return !HasFailure();
  }

  void ResetFeatureList() { scoped_feature_list_.reset(); }

  GURL GetLaunchingPageURL() {
    // We use "localhost" as the host of the launching page, so that it has a
    // different origin than that the of the app. The resolved URL of the
    // launching page would have the same host as that of the app, but the
    // URLs used in our NavigationThrottle are not resolved.
    return embedded_test_server()->GetURL("localhost", kLaunchingPagePath);
  }

 private:
  const Extension* test_bookmark_app_;
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list_;
};

// Tests that navigating to the Web App's app_url doesn't open a new window
// if features::kDesktopPWAWindowing is disabled before installing the app.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleBrowserTest,
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
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleBrowserTest,
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
class BookmarkAppNavigationThrottleTransitionBrowserTest
    : public BookmarkAppNavigationThrottleBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<std::string, ui::PageTransition>> {};

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    BookmarkAppNavigationThrottleTransitionBrowserTest,
    testing::Combine(testing::Values(kInScopeUrlPath, kOutOfScopeUrlPath),
                     testing::Range(ui::PAGE_TRANSITION_FIRST,
                                    ui::PAGE_TRANSITION_LAST_CORE)));

IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleTransitionBrowserTest,
                       MainFrameNavigations) {
  InstallTestBookmarkApp();

  GURL target_url = embedded_test_server()->GetURL(std::get<0>(GetParam()));
  ui::PageTransition transition = std::get<1>(GetParam());
  chrome::NavigateParams params(browser(), target_url, transition);

  if (!ui::PageTransitionIsMainFrame(transition)) {
    // Subframe navigations require a different setup. See
    // BookmarkAppNavigationThrottleBrowserTest.SubframeNavigation.
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
// after NavigationThrottles run. Because of this, our
// BookmarkAppNavigationThrottle will not see the transition type as
// PAGE_TRANSITION_AUTO_SUBFRAME/PAGE_TRANSITION_MANUAL_SUBFRAME, even though,
// by the end of the navigation, the transition type is
// PAGE_TRANSITION_AUTO_SUBFRAME/PAGE_TRANSITON_MANUAL_SUBFRAME.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleBrowserTest,
                       AutoSubframeNavigation) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  InsertIFrame(initial_tab);

  content::RenderFrameHost* iframe = GetIFrame(initial_tab);
  const GURL app_url = embedded_test_server()->GetURL(kAppUrlPath);

  chrome::NavigateParams params(browser(), app_url, ui::PAGE_TRANSITION_LINK);
  params.frame_tree_node_id = iframe->GetFrameTreeNodeId();
  content::TestFrameNavigationObserver observer(iframe);
  TestIFrameActionDoesNotOpenAppWindow(
      app_url, base::Bind(&NavigateToURLWrapper, &params));

  ASSERT_TRUE(ui::PageTransitionCoreTypeIs(observer.transition_type(),
                                           ui::PAGE_TRANSITION_AUTO_SUBFRAME));
}

IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleBrowserTest,
                       ManualSubframeNavigation) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  InsertIFrame(initial_tab);

  {
    // Navigate the iframe once, so that the next navigation is a
    // MANUAL_SUBFRAME navigation.
    content::RenderFrameHost* iframe = GetIFrame(initial_tab);
    chrome::NavigateParams params(browser(), GetLaunchingPageURL(),
                                  ui::PAGE_TRANSITION_LINK);
    params.frame_tree_node_id = iframe->GetFrameTreeNodeId();
    ASSERT_TRUE(TestIFrameActionDoesNotOpenAppWindow(
        GetLaunchingPageURL(), base::Bind(&NavigateToURLWrapper, &params)));
  }

  content::RenderFrameHost* iframe = GetIFrame(initial_tab);
  const GURL app_url = embedded_test_server()->GetURL(kAppUrlPath);

  chrome::NavigateParams params(browser(), app_url, ui::PAGE_TRANSITION_LINK);
  params.frame_tree_node_id = iframe->GetFrameTreeNodeId();
  content::TestFrameNavigationObserver observer(iframe);
  TestIFrameActionDoesNotOpenAppWindow(
      app_url, base::Bind(&NavigateToURLWrapper, &params));

  ASSERT_TRUE(ui::PageTransitionCoreTypeIs(
      observer.transition_type(), ui::PAGE_TRANSITION_MANUAL_SUBFRAME));
}

// Tests that clicking a link with target="_self" to the app's app_url opens the
// Bookmark App.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleBrowserTest, AppUrlSelf) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL app_url = embedded_test_server()->GetURL(kAppUrlPath);
  TestTabActionOpensAppWindow(
      app_url, base::Bind(&ClickLinkAndWait,
                          browser()->tab_strip_model()->GetActiveWebContents(),
                          app_url, LinkTarget::SELF));
}

// Tests that clicking a link with target="_self" and for which the server
// redirects to the app's app_url opens the Bookmark App.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleBrowserTest,
                       ServerRedirectToAppUrlSelf) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL app_url = embedded_test_server()->GetURL(kAppUrlPath);
  const GURL redirecting_url = embedded_test_server()->GetURL(
      "localhost", CreateServerRedirect(app_url));
  TestTabActionOpensAppWindow(
      app_url, base::Bind(&ClickLinkAndWaitForURL,
                          browser()->tab_strip_model()->GetActiveWebContents(),
                          redirecting_url, app_url, LinkTarget::SELF));
}

// Tests that clicking a link with target="_self" and for which the client
// redirects to the app's app_url opens the Bookmark App.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleBrowserTest,
                       ClientRedirectToAppUrlSelf) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL initial_url = initial_tab->GetLastCommittedURL();
  int num_tabs = browser()->tab_strip_model()->count();
  size_t num_browsers = chrome::GetBrowserCount(profile());

  const GURL app_url = embedded_test_server()->GetURL(kAppUrlPath);
  const GURL redirecting_url = embedded_test_server()->GetURL(
      "localhost", CreateClientRedirect(app_url));
  ClickLinkAndWaitForURL(browser()->tab_strip_model()->GetActiveWebContents(),
                         redirecting_url, app_url, LinkTarget::SELF);

  EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
  EXPECT_EQ(++num_browsers, chrome::GetBrowserCount(profile()));
  EXPECT_NE(browser(), chrome::FindLastActive());

  EXPECT_EQ(redirecting_url, initial_tab->GetLastCommittedURL());
  EXPECT_EQ(app_url, chrome::FindLastActive()
                         ->tab_strip_model()
                         ->GetActiveWebContents()
                         ->GetLastCommittedURL());
}

// Tests that clicking a link with target="_self" to a URL in the Web App's
// scope opens a new browser window.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleBrowserTest,
                       InScopeUrlSelf) {
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
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleBrowserTest,
                       OutOfScopeUrlSelf) {
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

// Tests that submitting a form using POST does not open a new app window.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleBrowserTest,
                       PostFormSubmission) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL in_scope_url = embedded_test_server()->GetURL(kInScopeUrlPath);
  TestTabActionDoesNotOpenAppWindow(
      in_scope_url,
      base::Bind(&SubmitFormAndWait,
                 browser()->tab_strip_model()->GetActiveWebContents(),
                 in_scope_url, net::URLFetcher::RequestType::POST));
}

// Tests that submitting a form using GET does not open a new app window.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleBrowserTest,
                       GetFormSubmission) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  GURL::Replacements replacements;
  replacements.SetQuery("", url::Component(0, 0));
  const GURL in_scope_form_url = embedded_test_server()
                                     ->GetURL(kInScopeUrlPath)
                                     .ReplaceComponents(replacements);
  TestTabActionDoesNotOpenAppWindow(
      in_scope_form_url,
      base::Bind(&SubmitFormAndWait,
                 browser()->tab_strip_model()->GetActiveWebContents(),
                 in_scope_form_url, net::URLFetcher::RequestType::GET));
}

// Tests that prerender links don't open the app.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleBrowserTest,
                       PrerenderLinks) {
  prerender::PrerenderManager::SetMode(
      prerender::PrerenderManager::PRERENDER_MODE_ENABLED);

  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  TestTabActionDoesNotNavigateOrOpenAppWindow(base::Bind(
      [](content::WebContents* web_contents, const GURL& target_url) {
        std::string script = base::StringPrintf(
            "(() => {"
            "const prerender_link = document.createElement('link');"
            "prerender_link.rel = 'prerender';"
            "prerender_link.href = '%s';"
            "prerender_link.addEventListener('webkitprerenderstop',"
            "() => window.domAutomationController.send(true));"
            "document.body.appendChild(prerender_link);"
            "})();",
            target_url.spec().c_str());
        bool result;
        ASSERT_TRUE(content::ExecuteScriptAndExtractBool(web_contents, script,
                                                         &result));
        ASSERT_TRUE(result);
      },
      browser()->tab_strip_model()->GetActiveWebContents(),
      embedded_test_server()->GetURL(kInScopeUrlPath)));
}

// Tests fetch calls don't open a new App window.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleBrowserTest, Fetch) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  TestTabActionDoesNotNavigateOrOpenAppWindow(base::Bind(
      [](content::WebContents* web_contents, const GURL& target_url) {
        std::string script = base::StringPrintf(
            "(() => {"
            "fetch('%s').then(response => {"
            "  window.domAutomationController.send(response.ok);"
            "});"
            "})();",
            target_url.spec().c_str());
        bool result;
        ASSERT_TRUE(content::ExecuteScriptAndExtractBool(web_contents, script,
                                                         &result));
        ASSERT_TRUE(result);
      },
      browser()->tab_strip_model()->GetActiveWebContents(),
      embedded_test_server()->GetURL(kInScopeUrlPath)));
}

// Tests that clicking "Open link in incognito window" to an in-scope URL opens
// an incognito window and not an app window.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleBrowserTest,
                       OpenInIncognito) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  size_t num_browsers = chrome::GetBrowserCount(profile());
  int num_tabs = browser()->tab_strip_model()->count();
  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL initial_url = initial_tab->GetLastCommittedURL();

  const GURL in_scope_url = embedded_test_server()->GetURL(kInScopeUrlPath);
  auto observer = GetTestNavigationObserver(in_scope_url);
  content::ContextMenuParams params;
  params.page_url = initial_url;
  params.link_url = in_scope_url;
  TestRenderViewContextMenu menu(initial_tab->GetMainFrame(), params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKOFFTHERECORD,
                      0 /* event_flags */);
  observer->WaitForNavigationFinished();

  Browser* incognito_browser = chrome::FindLastActive();
  EXPECT_EQ(incognito_browser->profile(), profile()->GetOffTheRecordProfile());
  EXPECT_NE(browser(), incognito_browser);
  EXPECT_EQ(in_scope_url, incognito_browser->tab_strip_model()
                              ->GetActiveWebContents()
                              ->GetLastCommittedURL());

  EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
  EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
  EXPECT_EQ(initial_tab, browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(initial_url, initial_tab->GetLastCommittedURL());
}

// Tests that clicking a link to an in-scope URL when in incognito does not open
// an App window.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleBrowserTest,
                       InScopeUrlIncognito) {
  InstallTestBookmarkApp();
  Browser* incognito_browser = CreateIncognitoBrowser();
  NavigateToLaunchingPage(incognito_browser);

  const GURL in_scope_url = embedded_test_server()->GetURL(kInScopeUrlPath);
  TestTabActionDoesNotOpenAppWindow(
      incognito_browser, in_scope_url,
      base::Bind(&ClickLinkAndWait,
                 incognito_browser->tab_strip_model()->GetActiveWebContents(),
                 in_scope_url, LinkTarget::SELF));
}

// Tests that clicking links inside a website for an installed app doesn't open
// a new browser window.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleBrowserTest,
                       InWebsiteNavigation) {
  InstallTestBookmarkApp();

  // Navigate to app's page. Shouldn't open a new window.
  const GURL app_url = embedded_test_server()->GetURL(kAppUrlPath);
  chrome::NavigateParams params(browser(), app_url, ui::PAGE_TRANSITION_TYPED);
  ASSERT_TRUE(TestTabActionDoesNotOpenAppWindow(
      app_url, base::Bind(&NavigateToURLWrapper, &params)));

  const GURL in_scope_url = embedded_test_server()->GetURL(kInScopeUrlPath);
  TestTabActionDoesNotOpenAppWindow(
      in_scope_url,
      base::Bind(&ClickLinkAndWait,
                 browser()->tab_strip_model()->GetActiveWebContents(),
                 in_scope_url, LinkTarget::SELF));
}

// Tests that clicking links inside the app doesn't open new browser windows.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleBrowserTest,
                       InAppNavigation) {
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
