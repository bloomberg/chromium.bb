// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/bookmark_app_helper.h"
#include "chrome/browser/extensions/bookmark_app_navigation_throttle_utils.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ssl/cert_verifier_browser_test.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/notification_types.h"
#include "extensions/common/constants.h"
#include "net/base/escape.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_fetcher.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace net::test_server;

namespace extensions {

enum class LinkTarget {
  SELF,
  BLANK,
};

enum class StartIn {
  BROWSER,
  APP,
};

enum class WindowAccessResult {
  CAN_ACCESS,
  CANNOT_ACCESS,
  // Used for when there was an unexpected issue when accessing the other window
  // e.g. there is no other window, or the other window's URL does not match the
  // expected URL.
  OTHER,
};

namespace {

const char kQueryParam[] = "test=";
const char kQueryParamName[] = "test";

// On macOS the Meta key is used as a modifier instead of Control.
#if defined(OS_MACOSX)
constexpr int kCtrlOrMeta = blink::WebInputEvent::kMetaKey;
#else
constexpr int kCtrlOrMeta = blink::WebInputEvent::kControlKey;
#endif

// Subclass of TestNavigationObserver that saves extra information about the
// navigation and watches all navigations to |target_url|.
class BookmarkAppNavigationObserver : public content::TestNavigationObserver {
 public:
  // Creates an observer that watches navigations to |target_url| on all
  // existing and newly added WebContents.
  explicit BookmarkAppNavigationObserver(const GURL& target_url)
      : content::TestNavigationObserver(target_url),
        last_navigation_is_post_(false) {
    WatchExistingWebContents();
    StartWatchingNewWebContents();
  }

  bool last_navigation_is_post() const { return last_navigation_is_post_; }

  const scoped_refptr<network::ResourceRequestBody>&
  last_resource_request_body() const {
    return last_resource_request_body_;
  }

 private:
  void OnDidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    last_navigation_is_post_ = navigation_handle->IsPost();
    last_resource_request_body_ = navigation_handle->GetResourceRequestBody();
    content::TestNavigationObserver::OnDidFinishNavigation(navigation_handle);
  }

  // True if the last navigation was a post.
  bool last_navigation_is_post_;

  // The request body of the last navigation if it was a post request.
  scoped_refptr<network::ResourceRequestBody> last_resource_request_body_;
};

bool HasOpenedWindowAndOpener(content::WebContents* opener_contents,
                              content::WebContents* opened_contents) {
  bool has_opener;
  CHECK(content::ExecuteScriptAndExtractBool(
      opened_contents, "window.domAutomationController.send(!!window.opener);",
      &has_opener));

  bool has_openedWindow;
  CHECK(content::ExecuteScriptAndExtractBool(
      opener_contents,
      "window.domAutomationController.send(!!window.openedWindow.window)",
      &has_openedWindow));

  return has_opener && has_openedWindow;
}

void ExpectNavigationResultHistogramEquals(
    const base::HistogramTester& histogram_tester,
    const std::vector<std::pair<BookmarkAppNavigationThrottleResult,
                                base::HistogramBase::Count>>& expected_counts) {
  std::vector<base::Bucket> expected_bucket_counts;
  for (const auto& pair : expected_counts) {
    expected_bucket_counts.push_back(base::Bucket(
        static_cast<base::HistogramBase::Sample>(pair.first), pair.second));
  }

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Extensions.BookmarkApp.NavigationResult"),
      testing::UnorderedElementsAreArray(expected_bucket_counts));
}

// When an app is launched, whether it's in response to a navigation or click
// in a launch surface e.g. App Shelf, the first navigation in the app is
// an AUTO_BOOKMARK navigation.
std::pair<BookmarkAppNavigationThrottleResult, base::HistogramBase::Count>
GetAppLaunchedEntry() {
  return {BookmarkAppNavigationThrottleResult::kProceedTransitionAutoBookmark,
          1};
}

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

void ExecuteContextMenuLinkCommandAndWait(content::WebContents* web_contents,
                                          const GURL& target_url,
                                          int command_id) {
  auto observer = GetTestNavigationObserver(target_url);
  content::ContextMenuParams params;
  params.page_url = web_contents->GetLastCommittedURL();
  params.link_url = target_url;
  TestRenderViewContextMenu menu(web_contents->GetMainFrame(), params);
  menu.Init();
  menu.ExecuteCommand(command_id, 0 /* event_flags */);
  observer->WaitForNavigationFinished();
}

// Calls window.open() with |target_url| on the main frame of |web_contents|.
// Returns once the new window has navigated to |target_url|.
void WindowOpenAndWait(content::WebContents* web_contents,
                       const GURL& target_url) {
  auto observer = GetTestNavigationObserver(target_url);
  const std::string script = base::StringPrintf(
      "(() => {"
      "  window.openedWindow = window.open('%s');"
      "})();",
      target_url.spec().c_str());
  ASSERT_TRUE(content::ExecuteScript(web_contents, script));
  observer->WaitForNavigationFinished();
}

// Calls window.open() with |target_url| on the main frame of |web_contents|.
// Returns true if the resulting child window is allowed to access members of
// its opener.
WindowAccessResult CanChildWindowAccessOpener(
    content::WebContents* web_contents,
    const GURL& target_url) {
  WindowOpenAndWait(web_contents, target_url);

  content::WebContents* new_contents =
      chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();

  const std::string script = base::StringPrintf(
      "(() => {"
      "  const [CAN_ACCESS, CANNOT_ACCESS, OTHER] = [0, 1, 2];"
      "  let result = OTHER;"
      "  try {"
      "    if (window.opener.location.href === '%s')"
      "      result = CAN_ACCESS;"
      "  } catch (e) {"
      "    if (e.name === 'SecurityError')"
      "      result = CANNOT_ACCESS;"
      "  }"
      "  window.domAutomationController.send(result);"
      "})();",
      web_contents->GetLastCommittedURL().spec().c_str());

  int access_result;
  CHECK(content::ExecuteScriptAndExtractInt(new_contents, script,
                                            &access_result));

  switch (access_result) {
    case 0:
      return WindowAccessResult::CAN_ACCESS;
    case 1:
      return WindowAccessResult::CANNOT_ACCESS;
    case 2:
      return WindowAccessResult::OTHER;
    default:
      NOTREACHED();
      return WindowAccessResult::OTHER;
  }
}

// Creates an <a> element, sets its href and target to |link_url| and |target|
// respectively, adds it to the DOM, and clicks on it with |modifiers|. Returns
// once |target_url| has loaded. |modifiers| should be based on
// blink::WebInputEvent::Modifiers.
void ClickLinkWithModifiersAndWaitForURL(content::WebContents* web_contents,
                                         const GURL& link_url,
                                         const GURL& target_url,
                                         LinkTarget target,
                                         const std::string& rel,
                                         int modifiers) {
  auto observer = GetTestNavigationObserver(target_url);
  std::string script = base::StringPrintf(
      "(() => {"
      "const link = document.createElement('a');"
      "link.href = '%s';"
      "link.target = '%s';"
      "link.rel = '%s';"
      // Make a click target that covers the whole viewport.
      "const click_target = document.createElement('textarea');"
      "click_target.position = 'absolute';"
      "click_target.top = 0;"
      "click_target.left = 0;"
      "click_target.style.height = '100vh';"
      "click_target.style.width = '100vw';"
      "link.appendChild(click_target);"
      "document.body.appendChild(link);"
      "})();",
      link_url.spec().c_str(), target == LinkTarget::SELF ? "_self" : "_blank",
      rel.c_str());
  ASSERT_TRUE(content::ExecuteScript(web_contents, script));

  content::SimulateMouseClick(web_contents, modifiers,
                              blink::WebMouseEvent::Button::kLeft);

  observer->WaitForNavigationFinished();
}

// Creates an <a> element, sets its href and target to |link_url| and |target|
// respectively, adds it to the DOM, and clicks on it. Returns once |target_url|
// has loaded.
void ClickLinkAndWaitForURL(content::WebContents* web_contents,
                            const GURL& link_url,
                            const GURL& target_url,
                            LinkTarget target,
                            const std::string& rel) {
  ClickLinkWithModifiersAndWaitForURL(
      web_contents, link_url, target_url, target, rel,
      blink::WebInputEvent::Modifiers::kNoModifiers);
}

// Creates an <a> element, sets its href and target to |link_url| and |target|
// respectively, adds it to the DOM, and clicks on it. Returns once the link
// has loaded.
void ClickLinkAndWait(content::WebContents* web_contents,
                      const GURL& link_url,
                      LinkTarget target,
                      const std::string& rel) {
  ClickLinkAndWaitForURL(web_contents, link_url, link_url, target, rel);
}

// Creates an <a> element, sets its href and target to |link_url| and |target|
// respectively, adds it to the DOM, and clicks on it with |modifiers|. Returns
// once the link has loaded. |modifiers| should be based on
// blink::WebInputEvent::Modifiers.
void ClickLinkWithModifiersAndWait(content::WebContents* web_contents,
                                   const GURL& link_url,
                                   LinkTarget target,
                                   const std::string& rel,
                                   int modifiers) {
  ClickLinkWithModifiersAndWaitForURL(web_contents, link_url, link_url, target,
                                      rel, modifiers);
}

// Adds a query parameter to |base_url|.
GURL AddTestQueryParam(const GURL& base_url) {
  GURL::Replacements replacements;
  std::string query(kQueryParam);
  replacements.SetQuery(query.c_str(), url::Component(0, query.length()));
  return base_url.ReplaceComponents(replacements);
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
  bool is_post = true;
  if (method == net::URLFetcher::RequestType::GET) {
    is_post = false;
    ASSERT_TRUE(target_url.has_query());
    ASSERT_EQ(kQueryParam, target_url.query());
  }

  BookmarkAppNavigationObserver observer(target_url);
  std::string script = base::StringPrintf(
      "(() => {"
      "const form = document.createElement('form');"
      "form.action = '%s';"
      "form.method = '%s';"
      "const input = document.createElement('input');"
      "input.name = '%s';"
      "form.appendChild(input);"
      "const button = document.createElement('input');"
      "button.type = 'submit';"
      "form.appendChild(button);"
      "document.body.appendChild(form);"
      "button.dispatchEvent(new MouseEvent('click', {'view': window}));"
      "})();",
      target_url.spec().c_str(),
      method == net::URLFetcher::RequestType::POST ? "post" : "get",
      kQueryParamName);
  ASSERT_TRUE(content::ExecuteScript(web_contents, script));
  observer.WaitForNavigationFinished();

  EXPECT_EQ(is_post, observer.last_navigation_is_post());
  if (is_post) {
    const std::vector<network::DataElement>* elements =
        observer.last_resource_request_body()->elements();
    EXPECT_EQ(1u, elements->size());
    const auto& element = elements->front();
    EXPECT_EQ(kQueryParam, std::string(element.bytes(), element.length()));
  }
}

// Uses |params| to navigate to a URL. Blocks until the URL is loaded.
void NavigateToURLAndWait(NavigateParams* params) {
  auto observer = GetTestNavigationObserver(params->url);
  ui_test_utils::NavigateToURL(params);
  observer->WaitForNavigationFinished();
}

// Wrapper so that we can use base::BindOnce with NavigateToURL.
void NavigateToURLWrapper(NavigateParams* params) {
  ui_test_utils::NavigateToURL(params);
}

}  // namespace

const char kLaunchingPageHost[] = "launching-page.com";
const char kLaunchingPagePath[] = "/index.html";

const char kAppUrlHost[]        = "app.com";
const char kOtherAppUrlHost[]   = "other-app.com";
const char kAppScopePath[]      = "/in_scope/";
const char kAppUrlPath[]        = "/in_scope/index.html";
const char kInScopeUrlPath[]    = "/in_scope/other.html";
const char kOutOfScopeUrlPath[] = "/out_of_scope/index.html";

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data");

class BookmarkAppNavigationThrottleBaseBrowserTest
    : public ExtensionBrowserTest {
 public:
  BookmarkAppNavigationThrottleBaseBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        mock_cert_verifier_(),
        cert_verifier_(&mock_cert_verifier_) {}

  void SetUp() override {
    https_server_.AddDefaultHandlers(base::FilePath(kDocRoot));
    // Register a request handler that will return empty pages. Tests are
    // responsible for adding elements and firing events on these empty pages.
    https_server_.RegisterRequestHandler(
        base::BindRepeating([](const HttpRequest& request) {
          // Let the default request handlers handle redirections.
          if (request.GetURL().path() == "/server-redirect" ||
              request.GetURL().path() == "/client-redirect") {
            return std::unique_ptr<HttpResponse>();
          }
          auto response = std::make_unique<BasicHttpResponse>();
          response->set_content_type("text/html");
          response->AddCustomHeader("Access-Control-Allow-Origin", "*");
          return static_cast<std::unique_ptr<HttpResponse>>(
              std::move(response));
        }));

    ExtensionBrowserTest::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    ExtensionBrowserTest::SetUpInProcessBrowserTestFixture();
    ProfileIOData::SetCertVerifierForTesting(&mock_cert_verifier_);
  }

  void TearDownInProcessBrowserTestFixture() override {
    ExtensionBrowserTest::TearDownInProcessBrowserTestFixture();
    ProfileIOData::SetCertVerifierForTesting(nullptr);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kUseMockCertVerifierForTesting);
  }

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    // By default, all SSL cert checks are valid. Can be overriden in tests.
    cert_verifier_.set_default_result(net::OK);
  }

  void InstallTestBookmarkApp() {
    test_bookmark_app_ = InstallTestBookmarkApp(kAppUrlHost);
  }

  void InstallOtherTestBookmarkApp() {
    InstallTestBookmarkApp(kOtherAppUrlHost);
  }

  const Extension* InstallTestBookmarkApp(const std::string& app_host) {
    if (!https_server_.Started()) {
      CHECK(https_server_.Start());
    }

    WebApplicationInfo web_app_info;
    web_app_info.app_url = https_server_.GetURL(app_host, kAppUrlPath);
    web_app_info.scope = https_server_.GetURL(app_host, kAppScopePath);
    web_app_info.title = base::UTF8ToUTF16("Test app");
    web_app_info.description = base::UTF8ToUTF16("Test description");
    web_app_info.open_as_window = true;

    return InstallBookmarkApp(web_app_info);
  }

  // Installs a Bookmark App that immediately redirects to a URL with
  // |target_host| and |target_path|.
  const Extension* InstallImmediateRedirectingApp(
      const std::string& target_host,
      const std::string& target_path) {
    EXPECT_TRUE(https_server_.Start());
    const GURL target_url = https_server_.GetURL(target_host, target_path);

    WebApplicationInfo web_app_info;
    web_app_info.app_url =
        https_server_.GetURL(kAppUrlHost, CreateServerRedirect(target_url));
    web_app_info.scope = https_server_.GetURL(kAppUrlHost, "/");
    web_app_info.title = base::UTF8ToUTF16("Redirecting Test app");
    web_app_info.description = base::UTF8ToUTF16("Test description");
    web_app_info.open_as_window = true;

    return InstallBookmarkApp(web_app_info);
  }

  Browser* OpenTestBookmarkApp() {
    GURL app_url = https_server_.GetURL(kAppUrlHost, kAppUrlPath);
    auto observer = GetTestNavigationObserver(app_url);
    Browser* app_browser = LaunchAppBrowser(test_bookmark_app_);
    observer->WaitForNavigationFinished();

    return app_browser;
  }

  // Navigates the active tab in |browser| to the launching page.
  void NavigateToLaunchingPage(Browser* browser) {
    ui_test_utils::NavigateToURL(browser, GetLaunchingPageURL());
  }

  // Navigates the active tab to the launching page.
  void NavigateToLaunchingPage() { NavigateToLaunchingPage(browser()); }

  // Navigates the browser's current active tab to the test app's URL. It does
  // not open a new app window.
  void NavigateToTestAppURL() {
    const GURL app_url = https_server_.GetURL(kAppUrlHost, kAppUrlPath);
    NavigateParams params(browser(), app_url, ui::PAGE_TRANSITION_TYPED);
    ASSERT_TRUE(TestTabActionDoesNotOpenAppWindow(
        app_url, base::BindOnce(&NavigateToURLWrapper, &params)));
  }

  // Checks that, after running |action|, the initial tab's window doesn't have
  // any new tabs, the initial tab did not navigate, and that no new windows
  // have been opened.
  void TestTabActionDoesNotNavigateOrOpenAppWindow(base::OnceClosure action) {
    size_t num_browsers = chrome::GetBrowserCount(profile());
    int num_tabs = browser()->tab_strip_model()->count();
    content::WebContents* initial_tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    GURL initial_url = initial_tab->GetLastCommittedURL();

    std::move(action).Run();

    EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
    EXPECT_EQ(browser(), chrome::FindLastActive());
    EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
    EXPECT_EQ(initial_tab,
              browser()->tab_strip_model()->GetActiveWebContents());
    EXPECT_EQ(initial_url, initial_tab->GetLastCommittedURL());
  }

  // Checks that, after running |action|, a new tab is opened with |target_url|,
  // the initial tab is still focused, and that the initial tab didn't
  // navigate.
  void TestTabActionOpensBackgroundTab(const GURL& target_url,
                                       base::OnceClosure action) {
    size_t num_browsers = chrome::GetBrowserCount(profile());
    int num_tabs = browser()->tab_strip_model()->count();
    content::WebContents* initial_tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    GURL initial_url = initial_tab->GetLastCommittedURL();

    std::move(action).Run();

    EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
    EXPECT_EQ(browser(), chrome::FindLastActive());
    EXPECT_EQ(++num_tabs, browser()->tab_strip_model()->count());
    EXPECT_EQ(initial_tab,
              browser()->tab_strip_model()->GetActiveWebContents());
    EXPECT_EQ(initial_url, initial_tab->GetLastCommittedURL());

    content::WebContents* new_tab =
        browser()->tab_strip_model()->GetWebContentsAt(num_tabs - 1);
    EXPECT_NE(new_tab, initial_tab);
    EXPECT_EQ(target_url, new_tab->GetLastCommittedURL());
  }

  // Checks that a new regular browser window is opened, that the new window
  // is in the foreground, and that the initial tab didn't navigate.
  void TestTabActionOpensForegroundWindow(const GURL& target_url,
                                          base::OnceClosure action) {
    size_t num_browsers = chrome::GetBrowserCount(profile());
    int num_tabs = browser()->tab_strip_model()->count();
    content::WebContents* initial_tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    GURL initial_url = initial_tab->GetLastCommittedURL();

    std::move(action).Run();

    EXPECT_EQ(++num_browsers, chrome::GetBrowserCount(profile()));

    Browser* new_window = chrome::FindLastActive();
    EXPECT_NE(new_window, browser());
    EXPECT_FALSE(new_window->is_app());
    EXPECT_EQ(target_url, new_window->tab_strip_model()
                              ->GetActiveWebContents()
                              ->GetLastCommittedURL());

    EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
    EXPECT_EQ(initial_url, initial_tab->GetLastCommittedURL());
  }

  // Checks that, after running |action|, the initial tab's window doesn't have
  // any new tabs, the initial tab did not navigate, and that a new app window
  // with |target_url| is opened.
  void TestTabActionOpensAppWindow(const GURL& target_url,
                                   base::OnceClosure action) {
    content::WebContents* initial_tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    GURL initial_url = initial_tab->GetLastCommittedURL();
    int num_tabs = browser()->tab_strip_model()->count();
    size_t num_browsers = chrome::GetBrowserCount(profile());

    std::move(action).Run();

    EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
    EXPECT_EQ(++num_browsers, chrome::GetBrowserCount(profile()));
    EXPECT_NE(browser(), chrome::FindLastActive());

    EXPECT_EQ(initial_url, initial_tab->GetLastCommittedURL());
    EXPECT_EQ(target_url, chrome::FindLastActive()
                              ->tab_strip_model()
                              ->GetActiveWebContents()
                              ->GetLastCommittedURL());
  }

  // Same as TestTabActionOpensAppWindow(), but also tests that the newly opened
  // app window has an opener.
  void TestTabActionOpensAppWindowWithOpener(const GURL& target_url,
                                             base::OnceClosure action) {
    TestTabActionOpensAppWindow(target_url, std::move(action));

    content::WebContents* initial_web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::WebContents* app_web_contents =
        chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();

    EXPECT_TRUE(
        HasOpenedWindowAndOpener(initial_web_contents, app_web_contents));
  }

  // Checks that no new windows are opened after running |action| and that the
  // existing |browser| window is still the active one and navigated to
  // |target_url|. Returns true if there were no errors.
  bool TestActionDoesNotOpenAppWindow(Browser* browser,
                                      const GURL& target_url,
                                      base::OnceClosure action) {
    content::WebContents* initial_tab =
        browser->tab_strip_model()->GetActiveWebContents();
    int num_tabs = browser->tab_strip_model()->count();
    size_t num_browsers = chrome::GetBrowserCount(browser->profile());

    std::move(action).Run();

    EXPECT_EQ(num_tabs, browser->tab_strip_model()->count());
    EXPECT_EQ(num_browsers, chrome::GetBrowserCount(browser->profile()));
    EXPECT_EQ(browser, chrome::FindLastActive());
    EXPECT_EQ(initial_tab, browser->tab_strip_model()->GetActiveWebContents());
    EXPECT_EQ(target_url, initial_tab->GetLastCommittedURL());

    return !HasFailure();
  }

  // Checks that a new foreground tab is opened in an existing browser, that the
  // new tab's browser is in the foreground, and that |app_browser| didn't
  // navigate.
  void TestAppActionOpensForegroundTab(Browser* app_browser,
                                       const GURL& target_url,
                                       base::OnceClosure action) {
    size_t num_browsers = chrome::GetBrowserCount(profile());
    int num_tabs_browser = browser()->tab_strip_model()->count();
    int num_tabs_app_browser = app_browser->tab_strip_model()->count();

    content::WebContents* app_web_contents =
        app_browser->tab_strip_model()->GetActiveWebContents();
    content::WebContents* initial_tab =
        browser()->tab_strip_model()->GetActiveWebContents();

    GURL initial_app_url = app_web_contents->GetLastCommittedURL();
    GURL initial_tab_url = initial_tab->GetLastCommittedURL();

    std::move(action).Run();

    EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));

    EXPECT_EQ(browser(), chrome::FindLastActive());

    EXPECT_EQ(++num_tabs_browser, browser()->tab_strip_model()->count());
    EXPECT_EQ(num_tabs_app_browser, app_browser->tab_strip_model()->count());

    EXPECT_EQ(initial_app_url, app_web_contents->GetLastCommittedURL());

    content::WebContents* new_tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_NE(initial_tab, new_tab);
    EXPECT_EQ(target_url, new_tab->GetLastCommittedURL());
  }

  // Checks that a new app window is opened, that the new window is in the
  // foreground, that the |app_browser| didn't navigate and that the new window
  // has an opener.
  void TestAppActionOpensAppWindowWithOpener(Browser* app_browser,
                                             const GURL& target_url,
                                             base::OnceClosure action) {
    size_t num_browsers = chrome::GetBrowserCount(profile());
    int num_tabs_browser = browser()->tab_strip_model()->count();
    int num_tabs_app_browser = app_browser->tab_strip_model()->count();

    content::WebContents* app_web_contents =
        app_browser->tab_strip_model()->GetActiveWebContents();
    content::WebContents* initial_tab =
        browser()->tab_strip_model()->GetActiveWebContents();

    GURL initial_app_url = app_web_contents->GetLastCommittedURL();
    GURL initial_tab_url = initial_tab->GetLastCommittedURL();

    std::move(action).Run();

    EXPECT_EQ(++num_browsers, chrome::GetBrowserCount(profile()));

    Browser* new_app_browser = chrome::FindLastActive();
    EXPECT_NE(new_app_browser, browser());
    EXPECT_NE(new_app_browser, app_browser);

    EXPECT_EQ(num_tabs_browser, browser()->tab_strip_model()->count());
    EXPECT_EQ(num_tabs_app_browser, app_browser->tab_strip_model()->count());

    EXPECT_EQ(initial_app_url, app_web_contents->GetLastCommittedURL());

    content::WebContents* new_app_web_contents =
        new_app_browser->tab_strip_model()->GetActiveWebContents();
    EXPECT_EQ(target_url, new_app_web_contents->GetLastCommittedURL());

    EXPECT_TRUE(
        HasOpenedWindowAndOpener(app_web_contents, new_app_web_contents));
  }

  // Checks that no new windows are opened after running |action| and that the
  // main browser window is still the active one and navigated to |target_url|.
  // Returns true if there were no errors.
  bool TestTabActionDoesNotOpenAppWindow(const GURL& target_url,
                                         base::OnceClosure action) {
    return TestActionDoesNotOpenAppWindow(browser(), target_url,
                                          std::move(action));
  }

  // Checks that no new windows are opened after running |action| and that the
  // iframe in the initial tab navigated to |target_url|. Returns true if there
  // were no errors.
  bool TestIFrameActionDoesNotOpenAppWindow(const GURL& target_url,
                                            base::OnceClosure action) {
    size_t num_browsers = chrome::GetBrowserCount(profile());
    int num_tabs = browser()->tab_strip_model()->count();
    content::WebContents* initial_tab =
        browser()->tab_strip_model()->GetActiveWebContents();

    std::move(action).Run();

    EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
    EXPECT_EQ(browser(), chrome::FindLastActive());
    EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());

    // When Site Isolation is enabled, navigating the iframe to a different
    // origin causes the original iframe's RenderFrameHost to be deleted.
    // So we retrieve the iframe's RenderFrameHost again.
    EXPECT_EQ(target_url, GetIFrame(initial_tab)->GetLastCommittedURL());

    return !HasFailure();
  }

  GURL GetLaunchingPageURL() {
    return https_server_.GetURL(kLaunchingPageHost, kLaunchingPagePath);
  }

  const base::HistogramTester& global_histogram() { return histogram_tester_; }

  const Extension* test_bookmark_app() { return test_bookmark_app_; }

  const net::EmbeddedTestServer& https_server() { return https_server_; }

  CertVerifierBrowserTest::CertVerifier& mock_cert_verifier() {
    return cert_verifier_;
  }

 private:
  net::EmbeddedTestServer https_server_;
  net::MockCertVerifier mock_cert_verifier_;
  // Similar to net::MockCertVerifier, but also updates the CertVerifier
  // used by the NetworkService. This is needed for when tests run with
  // the NetworkService enabled.
  CertVerifierBrowserTest::CertVerifier cert_verifier_;
  const Extension* test_bookmark_app_;
  base::HistogramTester histogram_tester_;
};

// Tests for the behavior when the kDesktopPWAsLinkCapturing flag is on.
class BookmarkAppNavigationThrottleExperimentalBrowserTest
    : public BookmarkAppNavigationThrottleBaseBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kDesktopPWAWindowing, features::kDesktopPWAsLinkCapturing},
        {});
    BookmarkAppNavigationThrottleBaseBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the result is the same regardless of the 'rel' attribute of links.
class BookmarkAppNavigationThrottleExperimentalLinkBrowserTest
    : public BookmarkAppNavigationThrottleExperimentalBrowserTest,
      public ::testing::WithParamInterface<std::string> {};

// Tests that navigating to the Web App's app_url doesn't open a new window
// if features::kDesktopPWAsLinkCapturing is disabled before installing the app.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       FeatureDisable_BeforeInstall) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {features::kDesktopPWAsLinkCapturing});
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL app_url = https_server().GetURL(kAppUrlHost, kAppUrlPath);
  TestTabActionDoesNotOpenAppWindow(
      app_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     app_url, LinkTarget::SELF, GetParam()));

  ExpectNavigationResultHistogramEquals(global_histogram(), {});
}

// Tests that navigating to the Web App's app_url doesn't open a new window
// if features::kDesktopPWAWindowing is disabled after installing the app.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       FeatureDisable_AfterInstall) {
  InstallTestBookmarkApp();
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({}, {features::kDesktopPWAsLinkCapturing});
  NavigateToLaunchingPage();

  const GURL app_url = https_server().GetURL(kAppUrlHost, kAppUrlPath);
  TestTabActionDoesNotOpenAppWindow(
      app_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     app_url, LinkTarget::SELF, GetParam()));

  ExpectNavigationResultHistogramEquals(global_histogram(), {});
}

// Tests that most transition types for navigations to in-scope or
// out-of-scope URLs do not result in new app windows.
class BookmarkAppNavigationThrottleExperimentalTransitionBrowserTest
    : public BookmarkAppNavigationThrottleExperimentalBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<std::string, ui::PageTransition>> {};

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    BookmarkAppNavigationThrottleExperimentalTransitionBrowserTest,
    testing::Combine(testing::Values(kInScopeUrlPath, kOutOfScopeUrlPath),
                     testing::Range(ui::PAGE_TRANSITION_FIRST,
                                    ui::PAGE_TRANSITION_LAST_CORE)));

IN_PROC_BROWSER_TEST_P(
    BookmarkAppNavigationThrottleExperimentalTransitionBrowserTest,
    MainFrameNavigations) {
  InstallTestBookmarkApp();

  GURL target_url = https_server().GetURL(kAppUrlHost, std::get<0>(GetParam()));
  ui::PageTransition transition = std::get<1>(GetParam());
  NavigateParams params(browser(), target_url, transition);

  if (!ui::PageTransitionIsMainFrame(transition)) {
    // Subframe navigations require a different setup. See
    // BookmarkAppNavigationThrottleExperimentalBrowserTest.SubframeNavigation.
    return;
  }

  if (ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_LINK, transition) &&
      target_url == https_server().GetURL(kAppUrlHost, kInScopeUrlPath)) {
    TestTabActionOpensAppWindow(target_url,
                                base::BindOnce(&NavigateToURLAndWait, &params));
  } else {
    TestTabActionDoesNotOpenAppWindow(
        target_url, base::BindOnce(&NavigateToURLAndWait, &params));
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
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       AutoSubframeNavigation) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  InsertIFrame(initial_tab);

  content::RenderFrameHost* iframe = GetIFrame(initial_tab);
  const GURL app_url = https_server().GetURL(kAppUrlHost, kAppUrlPath);

  NavigateParams params(browser(), app_url, ui::PAGE_TRANSITION_LINK);
  params.frame_tree_node_id = iframe->GetFrameTreeNodeId();
  content::TestFrameNavigationObserver observer(iframe);
  TestIFrameActionDoesNotOpenAppWindow(
      app_url, base::BindOnce(&NavigateToURLWrapper, &params));

  ASSERT_TRUE(ui::PageTransitionCoreTypeIs(observer.transition_type(),
                                           ui::PAGE_TRANSITION_AUTO_SUBFRAME));

  // Subframe navigations are completely ignored, so there should be no change
  // in the histogram.
  ExpectNavigationResultHistogramEquals(global_histogram(), {});
}

IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
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
    NavigateParams params(browser(), GetLaunchingPageURL(),
                          ui::PAGE_TRANSITION_LINK);
    params.frame_tree_node_id = iframe->GetFrameTreeNodeId();
    ASSERT_TRUE(TestIFrameActionDoesNotOpenAppWindow(
        GetLaunchingPageURL(), base::BindOnce(&NavigateToURLWrapper, &params)));
  }

  content::RenderFrameHost* iframe = GetIFrame(initial_tab);
  const GURL app_url = https_server().GetURL(kAppUrlHost, kAppUrlPath);

  NavigateParams params(browser(), app_url, ui::PAGE_TRANSITION_LINK);
  params.frame_tree_node_id = iframe->GetFrameTreeNodeId();
  content::TestFrameNavigationObserver observer(iframe);
  TestIFrameActionDoesNotOpenAppWindow(
      app_url, base::BindOnce(&NavigateToURLWrapper, &params));

  ASSERT_TRUE(ui::PageTransitionCoreTypeIs(
      observer.transition_type(), ui::PAGE_TRANSITION_MANUAL_SUBFRAME));

  // Subframe navigations are completely ignored, so there should be no change
  // in the histogram.
  ExpectNavigationResultHistogramEquals(global_histogram(), {});
}

// Tests that pasting an in-scope URL into the address bar and navigating to it,
// does not open an app window.
// https://crbug.com/782004
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       LinkNavigationFromAddressBar) {
  InstallTestBookmarkApp();

  const GURL app_url = https_server().GetURL(kAppUrlHost, kAppUrlPath);
  // Fake a navigation with a TRANSITION_LINK core type and a
  // TRANSITION_FROM_ADDRESS_BAR qualifier. This matches the transition that
  // results from pasting a URL into the address and navigating to it.
  NavigateParams params(
      browser(), app_url,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  ASSERT_TRUE(TestTabActionDoesNotOpenAppWindow(
      app_url, base::BindOnce(&NavigateToURLWrapper, &params)));

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kProceedTransitionFromAddressBar,
        1}});
}

// Tests that going back to an in-scope URL does not open a new app window.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       BackNavigation) {
  InstallTestBookmarkApp();
  NavigateToTestAppURL();

  // Navigate to an in-scope URL to generate a link navigation that didn't
  // get intercepted. The navigation won't get intercepted because the target
  // URL is in-scope of the app for the current URL.
  const GURL in_scope_url = https_server().GetURL(kAppUrlHost, kInScopeUrlPath);
  ASSERT_TRUE(TestTabActionDoesNotOpenAppWindow(
      in_scope_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     in_scope_url, LinkTarget::SELF, "" /* rel */)));

  // Navigate to an out-of-scope URL.
  {
    const GURL out_of_scope_url =
        https_server().GetURL(kAppUrlHost, kOutOfScopeUrlPath);
    NavigateParams params(browser(), out_of_scope_url,
                          ui::PAGE_TRANSITION_TYPED);
    ASSERT_TRUE(TestTabActionDoesNotOpenAppWindow(
        out_of_scope_url, base::BindOnce(&NavigateToURLWrapper, &params)));
  }

  base::HistogramTester scoped_histogram;
  TestTabActionDoesNotOpenAppWindow(
      in_scope_url,
      base::BindOnce(
          [](content::WebContents* web_contents, const GURL& in_scope_url) {
            auto observer = GetTestNavigationObserver(in_scope_url);
            web_contents->GetController().GoBack();
            observer->WaitForNavigationFinished();
          },
          browser()->tab_strip_model()->GetActiveWebContents(), in_scope_url));

  ExpectNavigationResultHistogramEquals(
      scoped_histogram,
      {{BookmarkAppNavigationThrottleResult::kProceedTransitionForwardBack,
        1}});
}

// Tests that clicking a link to an app that launches in a tab does not open a
// a new app window.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       TabApp) {
  InstallTestBookmarkApp();

  // Set a pref indicating that the user wants to launch in a regular tab.
  extensions::SetLaunchType(browser()->profile(), test_bookmark_app()->id(),
                            extensions::LAUNCH_TYPE_REGULAR);

  NavigateToLaunchingPage();

  const GURL app_url = https_server().GetURL(kAppUrlHost, kAppUrlPath);
  TestTabActionDoesNotOpenAppWindow(
      app_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     app_url, LinkTarget::SELF, GetParam()));

  // Non-windowed apps are not considered when looking for a target app, so it's
  // as if there was no app installed for the URL.
  ExpectNavigationResultHistogramEquals(global_histogram(), {});
}

// Tests that clicking a link with target="_self" to the app's app_url opens the
// Bookmark App.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       AppUrlSelf) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL app_url = https_server().GetURL(kAppUrlHost, kAppUrlPath);
  TestTabActionOpensAppWindow(
      app_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     app_url, LinkTarget::SELF, GetParam()));

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kCancelOpenedApp, 1},
       GetAppLaunchedEntry()});
}

// Tests that clicking a link with target="_blank" to the app's app_url opens
// the Bookmark App.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       AppUrlBlank) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL app_url = https_server().GetURL(kAppUrlHost, kAppUrlPath);
  TestTabActionOpensAppWindow(
      app_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     app_url, LinkTarget::BLANK, GetParam()));

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kDeferMovingContentsToNewAppWindow,
        1}});
}

// Tests that Ctrl + Clicking a link to the app's app_url opens a new background
// tab and not the app.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       AppUrlCtrlClick) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL app_url = https_server().GetURL(kAppUrlPath);
  TestTabActionOpensBackgroundTab(
      app_url,
      base::BindOnce(&ClickLinkWithModifiersAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     app_url, LinkTarget::SELF, GetParam(), kCtrlOrMeta));

  ExpectNavigationResultHistogramEquals(
      global_histogram(), {{BookmarkAppNavigationThrottleResult::
                                kProceedDispositionNewBackgroundTab,
                            1}});
}

// Tests that clicking a link with target="_self" and for which the server
// redirects to the app's app_url opens the Bookmark App.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       ServerRedirectToAppUrlSelf) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL app_url = https_server().GetURL(kAppUrlHost, kAppUrlPath);
  const GURL redirecting_url =
      https_server().GetURL(kLaunchingPageHost, CreateServerRedirect(app_url));
  TestTabActionOpensAppWindow(
      app_url,
      base::BindOnce(&ClickLinkAndWaitForURL,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     redirecting_url, app_url, LinkTarget::SELF, GetParam()));

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kCancelOpenedApp, 1},
       GetAppLaunchedEntry()});
}

// Tests that clicking a link with target="_blank" and for which the server
// redirects to the app's app_url opens the Bookmark App.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       ServerRedirectToAppUrlBlank) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL app_url = https_server().GetURL(kAppUrlHost, kAppUrlPath);
  const GURL redirecting_url =
      https_server().GetURL(kLaunchingPageHost, CreateServerRedirect(app_url));
  TestTabActionOpensAppWindow(
      app_url,
      base::BindOnce(&ClickLinkAndWaitForURL,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     redirecting_url, app_url, LinkTarget::BLANK, GetParam()));

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kDeferMovingContentsToNewAppWindow,
        1}});
}

// Tests that clicking a link with target="_self" and for which the client
// redirects to the app's app_url opens the Bookmark App. The initial tab will
// be left on the redirecting URL.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       ClientRedirectToAppUrlSelf) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL initial_url = initial_tab->GetLastCommittedURL();
  int num_tabs = browser()->tab_strip_model()->count();
  size_t num_browsers = chrome::GetBrowserCount(profile());

  const GURL app_url = https_server().GetURL(kAppUrlHost, kAppUrlPath);
  const GURL redirecting_url =
      https_server().GetURL(kLaunchingPageHost, CreateClientRedirect(app_url));
  ClickLinkAndWaitForURL(browser()->tab_strip_model()->GetActiveWebContents(),
                         redirecting_url, app_url, LinkTarget::SELF,
                         GetParam());

  EXPECT_EQ(num_tabs, browser()->tab_strip_model()->count());
  EXPECT_EQ(++num_browsers, chrome::GetBrowserCount(profile()));
  EXPECT_NE(browser(), chrome::FindLastActive());

  EXPECT_EQ(redirecting_url, initial_tab->GetLastCommittedURL());
  EXPECT_EQ(app_url, chrome::FindLastActive()
                         ->tab_strip_model()
                         ->GetActiveWebContents()
                         ->GetLastCommittedURL());

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kCancelOpenedApp, 1},
       GetAppLaunchedEntry()});
}

// Tests that clicking a link with target="_blank" and for which the client
// redirects to the app's app_url opens the Bookmark App. The new tab will be
// left on the redirecting URL.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       ClientRedirectToAppUrlBlank) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL initial_url = initial_tab->GetLastCommittedURL();
  int num_tabs = browser()->tab_strip_model()->count();
  size_t num_browsers = chrome::GetBrowserCount(profile());

  const GURL app_url = https_server().GetURL(kAppUrlHost, kAppUrlPath);
  const GURL redirecting_url =
      https_server().GetURL(kLaunchingPageHost, CreateClientRedirect(app_url));
  ClickLinkAndWaitForURL(browser()->tab_strip_model()->GetActiveWebContents(),
                         redirecting_url, app_url, LinkTarget::BLANK,
                         GetParam());

  EXPECT_EQ(++num_tabs, browser()->tab_strip_model()->count());
  EXPECT_EQ(++num_browsers, chrome::GetBrowserCount(profile()));
  EXPECT_NE(browser(), chrome::FindLastActive());

  content::WebContents* new_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(new_tab, initial_tab);

  EXPECT_EQ(redirecting_url, new_tab->GetLastCommittedURL());
  EXPECT_EQ(app_url, chrome::FindLastActive()
                         ->tab_strip_model()
                         ->GetActiveWebContents()
                         ->GetLastCommittedURL());

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kCancelOpenedApp, 1},
       GetAppLaunchedEntry()});
}

// Tests that clicking a link with target="_self" to a URL in the Web App's
// scope opens a new browser window.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       InScopeUrlSelf) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL in_scope_url = https_server().GetURL(kAppUrlHost, kInScopeUrlPath);
  TestTabActionOpensAppWindow(
      in_scope_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     in_scope_url, LinkTarget::SELF, GetParam()));

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kCancelOpenedApp, 1},
       GetAppLaunchedEntry()});
}

// Tests that clicking a link with target="_self" to a URL out of the Web App's
// scope but with the same origin doesn't open a new browser window.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       OutOfScopeUrlSelf) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL out_of_scope_url =
      https_server().GetURL(kAppUrlHost, kOutOfScopeUrlPath);
  TestTabActionDoesNotOpenAppWindow(
      out_of_scope_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     out_of_scope_url, LinkTarget::SELF, GetParam()));

  ExpectNavigationResultHistogramEquals(global_histogram(), {});
}

// Tests that prerender links don't open the app.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       PrerenderLinks) {
  prerender::PrerenderManager::SetMode(
      prerender::PrerenderManager::PRERENDER_MODE_ENABLED);

  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  TestTabActionDoesNotNavigateOrOpenAppWindow(base::BindOnce(
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
      https_server().GetURL(kAppUrlHost, kInScopeUrlPath)));

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kCancelPrerenderContents, 1}});
}

// Tests fetch calls don't open a new App window.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       Fetch) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  TestTabActionDoesNotNavigateOrOpenAppWindow(base::BindOnce(
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
      https_server().GetURL(kAppUrlHost, kInScopeUrlPath)));

  // Fetch requests don't go through our NavigationHandle.
  ExpectNavigationResultHistogramEquals(global_histogram(), {});
}

// Tests that clicking "Open link in new tab" to an in-scope URL opens a new
// tab in the background.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       ContextMenuNewTab) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL app_url = https_server().GetURL(kAppUrlHost, kAppUrlPath);
  TestTabActionOpensBackgroundTab(
      app_url,
      base::BindOnce(&ExecuteContextMenuLinkCommandAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     app_url, IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kProceedStartedFromContextMenu,
        1}});
}

// Tests that clicking "Open link in new window" to an in-scope URL opens a new
// window in the foreground.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       ContextMenuNewWindow) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  const GURL app_url = https_server().GetURL(kAppUrlHost, kAppUrlPath);
  TestTabActionOpensForegroundWindow(
      app_url,
      base::BindOnce(&ExecuteContextMenuLinkCommandAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     app_url, IDC_CONTENT_CONTEXT_OPENLINKNEWWINDOW));

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kProceedStartedFromContextMenu,
        1}});
}

// Tests that clicking "Open link in new tab" in an app to an in-scope URL opens
// a new foreground tab in a regular browser window.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       InAppContextMenuNewTab) {
  InstallTestBookmarkApp();
  Browser* app_browser = OpenTestBookmarkApp();

  base::HistogramTester scoped_histogram;
  const GURL app_url = https_server().GetURL(kAppUrlHost, kAppUrlPath);
  TestAppActionOpensForegroundTab(
      app_browser, app_url,
      base::BindOnce(&ExecuteContextMenuLinkCommandAndWait,
                     app_browser->tab_strip_model()->GetActiveWebContents(),
                     app_url, IDC_CONTENT_CONTEXT_OPENLINKNEWTAB));

  ExpectNavigationResultHistogramEquals(
      scoped_histogram,
      {{BookmarkAppNavigationThrottleResult::kProceedStartedFromContextMenu,
        1}});
}

// Tests that clicking "Open link in incognito window" to an in-scope URL opens
// an incognito window and not an app window.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       OpenInIncognito) {
  InstallTestBookmarkApp();
  NavigateToLaunchingPage();

  size_t num_browsers = chrome::GetBrowserCount(profile());
  int num_tabs = browser()->tab_strip_model()->count();
  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL initial_url = initial_tab->GetLastCommittedURL();

  const GURL in_scope_url = https_server().GetURL(kAppUrlHost, kInScopeUrlPath);
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

  // Incognito navigations are completely ignored.
  ExpectNavigationResultHistogramEquals(global_histogram(), {});
}

// Tests that clicking a link to an in-scope URL when in incognito does not open
// an App window.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       InScopeUrlIncognito) {
  InstallTestBookmarkApp();
  Browser* incognito_browser = CreateIncognitoBrowser();
  NavigateToLaunchingPage(incognito_browser);

  const GURL in_scope_url = https_server().GetURL(kAppUrlHost, kInScopeUrlPath);
  TestActionDoesNotOpenAppWindow(
      incognito_browser, in_scope_url,
      base::BindOnce(
          &ClickLinkAndWait,
          incognito_browser->tab_strip_model()->GetActiveWebContents(),
          in_scope_url, LinkTarget::SELF, GetParam()));

  // Incognito navigations are completely ignored.
  ExpectNavigationResultHistogramEquals(global_histogram(), {});
}

// Tests that clicking a target=_self link from a URL out of the Web App's scope
// but with the same origin to an in-scope URL results in a new App window.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       FromOutOfScopeUrlToInScopeUrlSelf) {
  InstallTestBookmarkApp();

  // Navigate to out-of-scope URL. Shouldn't open a new window.
  const GURL out_of_scope_url =
      https_server().GetURL(kAppUrlHost, kOutOfScopeUrlPath);
  NavigateParams params(browser(), out_of_scope_url, ui::PAGE_TRANSITION_TYPED);
  ASSERT_TRUE(TestTabActionDoesNotOpenAppWindow(
      out_of_scope_url, base::BindOnce(&NavigateToURLWrapper, &params)));

  const GURL in_scope_url = https_server().GetURL(kAppUrlHost, kAppUrlPath);
  TestTabActionOpensAppWindow(
      in_scope_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     in_scope_url, LinkTarget::SELF, GetParam()));

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kCancelOpenedApp, 1},
       GetAppLaunchedEntry()});
}

// Tests that clicking a target=_blank link from a URL out of the Web App's
// scope but with the same origin to an in-scope URL results in a new App
// window.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       FromOutOfScopeUrlToInScopeUrlBlank) {
  InstallTestBookmarkApp();

  // Navigate to out-of-scope URL. Shouldn't open a new window.
  const GURL out_of_scope_url =
      https_server().GetURL(kAppUrlHost, kOutOfScopeUrlPath);
  NavigateParams params(browser(), out_of_scope_url, ui::PAGE_TRANSITION_TYPED);
  ASSERT_TRUE(TestTabActionDoesNotOpenAppWindow(
      out_of_scope_url, base::BindOnce(&NavigateToURLWrapper, &params)));

  const GURL in_scope_url = https_server().GetURL(kAppUrlHost, kAppUrlPath);
  TestTabActionOpensAppWindow(
      in_scope_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     in_scope_url, LinkTarget::BLANK, GetParam()));

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kDeferMovingContentsToNewAppWindow,
        1}});
}

// Tests that clicking links inside a website for an installed app doesn't open
// a new browser window.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
                       InWebsiteNavigation) {
  InstallTestBookmarkApp();
  NavigateToTestAppURL();

  base::HistogramTester scoped_histogram;
  const GURL in_scope_url = https_server().GetURL(kAppUrlHost, kInScopeUrlPath);
  TestTabActionDoesNotOpenAppWindow(
      in_scope_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     in_scope_url, LinkTarget::SELF, GetParam()));

  ExpectNavigationResultHistogramEquals(
      scoped_histogram,
      {{BookmarkAppNavigationThrottleResult::kProceedInBrowserSameScope, 1}});
}

class BookmarkAppNavigationThrottleExperimentalWindowOpenBrowserTest
    : public BookmarkAppNavigationThrottleExperimentalBrowserTest,
      public ::testing::WithParamInterface<std::string> {};

// Tests that same-origin or cross-origin apps created with window.open() from
// a regular browser window have an opener.
IN_PROC_BROWSER_TEST_P(
    BookmarkAppNavigationThrottleExperimentalWindowOpenBrowserTest,
    WindowOpenInBrowser) {
  InstallTestBookmarkApp();
  InstallOtherTestBookmarkApp();

  NavigateToTestAppURL();

  // Call window.open() with |target_url|.
  const GURL target_url = https_server().GetURL(GetParam(), kAppUrlPath);
  TestTabActionOpensAppWindowWithOpener(
      target_url,
      base::BindOnce(&WindowOpenAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     target_url));
}

// Tests that same-origin or cross-origin apps created with window.open() from
// another app window have an opener.
IN_PROC_BROWSER_TEST_P(
    BookmarkAppNavigationThrottleExperimentalWindowOpenBrowserTest,
    WindowOpenInApp) {
  InstallTestBookmarkApp();
  InstallOtherTestBookmarkApp();

  // Open app window.
  Browser* app_browser = OpenTestBookmarkApp();
  content::WebContents* app_web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  // Call window.open() with |target_url|.
  const GURL target_url = https_server().GetURL(GetParam(), kAppUrlPath);
  TestAppActionOpensAppWindowWithOpener(
      app_browser, target_url,
      base::BindOnce(&WindowOpenAndWait, app_web_contents, target_url));
}

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    BookmarkAppNavigationThrottleExperimentalWindowOpenBrowserTest,
    testing::Values(kAppUrlHost, kOtherAppUrlHost));

// Tests that a child window can access its opener, when the opener is a regular
// browser tab.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       AccessOpenerBrowserWindowFromChildWindow) {
  InstallTestBookmarkApp();
  InstallOtherTestBookmarkApp();

  NavigateToTestAppURL();

  content::WebContents* current_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  const GURL app_url = https_server().GetURL(kAppUrlHost, kAppUrlPath);
  EXPECT_EQ(WindowAccessResult::CAN_ACCESS,
            CanChildWindowAccessOpener(current_tab, app_url));

  const GURL other_app_url =
      https_server().GetURL(kOtherAppUrlHost, kAppUrlPath);
  EXPECT_EQ(WindowAccessResult::CANNOT_ACCESS,
            CanChildWindowAccessOpener(current_tab, other_app_url));
}

// Tests that a child window can access its opener, when the opener is another
// app.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       AccessOpenerAppWindowFromChildWindow) {
  InstallTestBookmarkApp();
  InstallOtherTestBookmarkApp();

  // Open app window.
  Browser* app_browser = OpenTestBookmarkApp();
  content::WebContents* app_web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  const GURL app_url = https_server().GetURL(kAppUrlHost, kAppUrlPath);
  EXPECT_EQ(WindowAccessResult::CAN_ACCESS,
            CanChildWindowAccessOpener(app_web_contents, app_url));

  const GURL other_app_url =
      https_server().GetURL(kOtherAppUrlHost, kAppUrlPath);
  EXPECT_EQ(WindowAccessResult::CANNOT_ACCESS,
            CanChildWindowAccessOpener(app_web_contents, other_app_url));
}

// Tests that in-browser navigations with all the following characteristics
// don't open a new app window or move the tab:
//  1. Are to in-scope URLs
//  2. Result in a AUTO_BOOKMARK transtion
//  3. Redirect to another in-scope URL.
//
// Clicking on sites in the New Tab Page is one of the ways to trigger this
// type of navigation.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       AutoBookmarkInScopeRedirect) {
  const Extension* app =
      InstallImmediateRedirectingApp(kAppUrlHost, kInScopeUrlPath);

  const GURL app_url = AppLaunchInfo::GetFullLaunchURL(app);
  NavigateParams params(browser(), app_url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  TestTabActionDoesNotOpenAppWindow(
      https_server().GetURL(kAppUrlHost, kInScopeUrlPath),
      base::BindOnce(&NavigateToURLWrapper, &params));

  // The first AUTO_BOOKMARK navigation is the one we triggered and the second
  // one is the redirect.
  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kProceedTransitionAutoBookmark,
        2}});
}

// Tests that in-browser navigations with all the following characteristics
// don't open a new app window or move the tab:
//  1. Are to in-scope URLs
//  2. Result in a AUTO_BOOKMARK transtion
//  3. Redirect to an out-of-scope URL.
//
// Clicking on sites in the New Tab Page is one of the ways to trigger this
// type of navigation.
IN_PROC_BROWSER_TEST_F(BookmarkAppNavigationThrottleExperimentalBrowserTest,
                       AutoBookmarkOutOfScopeRedirect) {
  const Extension* app =
      InstallImmediateRedirectingApp(kLaunchingPageHost, kLaunchingPagePath);

  const GURL app_url = AppLaunchInfo::GetFullLaunchURL(app);
  NavigateParams params(browser(), app_url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  TestTabActionDoesNotOpenAppWindow(
      GetLaunchingPageURL(), base::BindOnce(&NavigateToURLWrapper, &params));

  ExpectNavigationResultHistogramEquals(
      global_histogram(),
      {{BookmarkAppNavigationThrottleResult::kProceedTransitionAutoBookmark,
        1}});
}

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    BookmarkAppNavigationThrottleExperimentalLinkBrowserTest,
    testing::Values("", "noopener", "noreferrer", "nofollow"));

// Base class for testing the behavior is the same regardless of the
// kDesktopPWAsLinkCapturing feature flag.
class BookmarkAppNavigationThrottleBaseCommonBrowserTest
    : public BookmarkAppNavigationThrottleBaseBrowserTest {
 protected:
  void EnableFeaturesForTest(bool should_enable_link_capturing) {
    if (should_enable_link_capturing) {
      scoped_feature_list_.InitWithFeatures(
          {features::kDesktopPWAWindowing, features::kDesktopPWAsLinkCapturing},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {features::kDesktopPWAWindowing},
          {features::kDesktopPWAsLinkCapturing});
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Class for testing the behavior is the same regardless of the
// kDesktopPWAsLinkCapturing feature flag.
class BookmarkAppNavigationThrottleCommonBrowserTest
    : public BookmarkAppNavigationThrottleBaseCommonBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    EnableFeaturesForTest(GetParam());
    BookmarkAppNavigationThrottleBaseCommonBrowserTest::SetUp();
  }
};

// Tests that an app that immediately redirects to an out-of-scope URL opens a
// new foreground tab.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleCommonBrowserTest,
                       ImmediateOutOfScopeRedirect) {
  size_t num_browsers = chrome::GetBrowserCount(profile());
  int num_tabs_browser = browser()->tab_strip_model()->count();

  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL initial_url = initial_tab->GetLastCommittedURL();

  const Extension* redirecting_app =
      InstallImmediateRedirectingApp(kLaunchingPageHost, kLaunchingPagePath);

  auto observer = GetTestNavigationObserver(GetLaunchingPageURL());
  LaunchAppBrowser(redirecting_app);
  observer->WaitForNavigationFinished();

  EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));

  EXPECT_EQ(browser(), chrome::FindLastActive());
  EXPECT_EQ(++num_tabs_browser, browser()->tab_strip_model()->count());

  EXPECT_EQ(initial_url, initial_tab->GetLastCommittedURL());

  content::WebContents* new_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(initial_tab, new_tab);
  EXPECT_EQ(GetLaunchingPageURL(), new_tab->GetLastCommittedURL());

  // When kDesktopPWAsLinkCapturing is enabled, app launches get histogrammed,
  // but when it's disabled, they don't.
  if (GetParam()) {
    ExpectNavigationResultHistogramEquals(
        global_histogram(), {GetAppLaunchedEntry(),
                             {BookmarkAppNavigationThrottleResult::
                                  kOpenInChromeProceedOutOfScopeLaunch,
                              1}});
  } else {
    ExpectNavigationResultHistogramEquals(
        global_histogram(), {{BookmarkAppNavigationThrottleResult::
                                  kOpenInChromeProceedOutOfScopeLaunch,
                              1}});
  }
}

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    BookmarkAppNavigationThrottleCommonBrowserTest,
    testing::Bool());

// Set of tests to make sure form submissions have the correct behavior
// regardless of the kDesktopPWAsLinkCapturing feature flag.
class BookmarkAppNavigationThrottleCommonFormSubmissionBrowserTest
    : public BookmarkAppNavigationThrottleBaseCommonBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<bool,
                     StartIn,
                     std::string,
                     net::URLFetcher::RequestType,
                     std::string>> {
 public:
  void SetUp() override {
    EnableFeaturesForTest(std::get<0>(GetParam()));
    BookmarkAppNavigationThrottleBaseCommonBrowserTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_P(
    BookmarkAppNavigationThrottleCommonFormSubmissionBrowserTest,
    FormSubmission) {
  bool should_enable_link_capturing;
  StartIn start_in;
  std::string start_url_path;
  net::URLFetcher::RequestType request_type;
  std::string target_url_path;
  std::tie(should_enable_link_capturing, start_in, start_url_path, request_type,
           target_url_path) = GetParam();

  InstallTestBookmarkApp();

  // Pick where the test should start.
  Browser* current_browser;
  if (start_in == StartIn::APP)
    current_browser = OpenTestBookmarkApp();
  else
    current_browser = browser();

  // If in a regular browser window, navigate to the start URL.
  if (start_in == StartIn::BROWSER) {
    GURL start_url;
    if (start_url_path == kLaunchingPagePath)
      start_url = GetLaunchingPageURL();
    else
      start_url = https_server().GetURL(kAppUrlHost, kAppUrlPath);

    NavigateParams params(current_browser, start_url,
                          ui::PAGE_TRANSITION_TYPED);
    ASSERT_TRUE(TestTabActionDoesNotOpenAppWindow(
        start_url, base::BindOnce(&NavigateToURLWrapper, &params)));
  } else if (start_url_path == kLaunchingPagePath) {
    // Return early here since the app window can't start with an out-of-scope
    // URL.
    return;
  }

  // If the submit method is GET then add an query params.
  GURL target_url = https_server().GetURL(kAppUrlHost, target_url_path);
  if (request_type == net::URLFetcher::RequestType::GET) {
    target_url = AddTestQueryParam(target_url);
  }

  base::HistogramTester scoped_histogram;

  // All form submissions that start in the browser should be kept in the
  // browser.
  if (start_in == StartIn::BROWSER) {
    TestActionDoesNotOpenAppWindow(
        current_browser, target_url,
        base::BindOnce(
            &SubmitFormAndWait,
            current_browser->tab_strip_model()->GetActiveWebContents(),
            target_url, request_type));

    // Navigations to out-of-scope URLs are considered regular navigations and
    // therefore not recorded. Nothing gets histogrammed when
    // kDesktopPWAsLinkCapturing is disabled, because the navigation is not
    // happening in an app window.
    if (target_url_path == kInScopeUrlPath && should_enable_link_capturing) {
      ExpectNavigationResultHistogramEquals(
          scoped_histogram, {{BookmarkAppNavigationThrottleResult::
                                  kProceedInBrowserFormSubmission,
                              1}});
    }
    return;
  }

  // When in an app, in-scope navigations should always be kept in the app
  // window.
  if (target_url_path == kInScopeUrlPath) {
    TestActionDoesNotOpenAppWindow(
        current_browser, target_url,
        base::BindOnce(
            &SubmitFormAndWait,
            current_browser->tab_strip_model()->GetActiveWebContents(),
            target_url, request_type));

    ExpectNavigationResultHistogramEquals(
        scoped_histogram,
        {{BookmarkAppNavigationThrottleResult::kProceedInAppSameScope, 1}});
    return;
  }

  // When in an app, out-of-scope navigations should always open a new
  // foreground tab.
  DCHECK_EQ(target_url_path, kOutOfScopeUrlPath);
  TestAppActionOpensForegroundTab(
      current_browser, target_url,
      base::BindOnce(&SubmitFormAndWait,
                     current_browser->tab_strip_model()->GetActiveWebContents(),
                     target_url, request_type));

  ExpectNavigationResultHistogramEquals(
      scoped_histogram,
      {{BookmarkAppNavigationThrottleResult::kDeferOpenNewTabInAppOutOfScope,
        1}});
}

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    BookmarkAppNavigationThrottleCommonFormSubmissionBrowserTest,
    testing::Combine(testing::Bool(),
                     testing::Values(StartIn::BROWSER, StartIn::APP),
                     testing::Values(kLaunchingPagePath, kAppUrlPath),
                     testing::Values(net::URLFetcher::RequestType::GET,
                                     net::URLFetcher::RequestType::POST),
                     testing::Values(kOutOfScopeUrlPath, kInScopeUrlPath)));

// Tests that the result is the same regardless of the 'rel' attribute of links
// and of the kDesktopPWAsLinkCapturing feature flag.
class BookmarkAppNavigationThrottleCommonLinkBrowserTest
    : public BookmarkAppNavigationThrottleBaseCommonBrowserTest,
      public ::testing::WithParamInterface<std::tuple<bool, std::string>> {
 public:
  void SetUp() override {
    EnableFeaturesForTest(std::get<0>(GetParam()));
    BookmarkAppNavigationThrottleBaseCommonBrowserTest::SetUp();
  }
};

// Tests that clicking links inside the app to in-scope URLs doesn't open new
// browser windows.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleCommonLinkBrowserTest,
                       InAppInScopeNavigation) {
  InstallTestBookmarkApp();
  Browser* app_browser = OpenTestBookmarkApp();
  content::WebContents* app_web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  int num_tabs_browser = browser()->tab_strip_model()->count();
  int num_tabs_app_browser = app_browser->tab_strip_model()->count();
  size_t num_browsers = chrome::GetBrowserCount(profile());

  base::HistogramTester scoped_histogram;
  const GURL in_scope_url = https_server().GetURL(kAppUrlHost, kInScopeUrlPath);
  ClickLinkAndWait(app_web_contents, in_scope_url, LinkTarget::SELF,
                   std::get<1>(GetParam()));

  EXPECT_EQ(num_tabs_browser, browser()->tab_strip_model()->count());
  EXPECT_EQ(num_tabs_app_browser, app_browser->tab_strip_model()->count());
  EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
  EXPECT_EQ(app_browser, chrome::FindLastActive());

  EXPECT_EQ(in_scope_url, app_web_contents->GetLastCommittedURL());

  ExpectNavigationResultHistogramEquals(
      scoped_histogram,
      {{BookmarkAppNavigationThrottleResult::kProceedInAppSameScope, 1}});
}

// Tests that clicking links inside the app to out-of-scope URLs opens a new
// tab in an existing browser window, instead of navigating the existing
// app window.
IN_PROC_BROWSER_TEST_P(BookmarkAppNavigationThrottleCommonLinkBrowserTest,
                       InAppOutOfScopeNavigation) {
  InstallTestBookmarkApp();
  Browser* app_browser = OpenTestBookmarkApp();

  const base::HistogramTester scoped_histogram;
  const GURL out_of_scope_url =
      https_server().GetURL(kAppUrlHost, kOutOfScopeUrlPath);
  TestAppActionOpensForegroundTab(
      app_browser, out_of_scope_url,
      base::BindOnce(&ClickLinkAndWait,
                     app_browser->tab_strip_model()->GetActiveWebContents(),
                     out_of_scope_url, LinkTarget::SELF,
                     std::get<1>(GetParam())));

  ExpectNavigationResultHistogramEquals(
      scoped_histogram,
      {{BookmarkAppNavigationThrottleResult::kDeferOpenNewTabInAppOutOfScope,
        1}});
}

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    BookmarkAppNavigationThrottleCommonLinkBrowserTest,
    testing::Combine(
        testing::Bool(),
        testing::Values("", "noopener", "noreferrer", "nofollow")));

}  // namespace extensions
