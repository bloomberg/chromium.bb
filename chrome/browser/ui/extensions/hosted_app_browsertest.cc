// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/test_extension_dir.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/apps/app_info_dialog.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/clipboard/clipboard.h"

#if defined(OS_MACOSX)
#include "chrome/common/chrome_features.h"
#endif

using content::RenderFrameHost;
using content::WebContents;
using extensions::Extension;

namespace {

constexpr const char kExampleURL[] = "http://example.org/";
constexpr const char kAppDotComManifest[] = R"( { "name": "Hosted App",
  "version": "1",
  "manifest_version": 2,
  "app": {
    "launch": {
      "web_url": "%s"
    },
    "urls": ["*://app.com/"]
  }
} )";

void NavigateToURLAndWait(Browser* browser, const GURL& url) {
  content::TestNavigationObserver observer(
      browser->tab_strip_model()->GetActiveWebContents(),
      content::MessageLoopRunner::QuitMode::DEFERRED);
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);
  observer.Wait();
}

// Used by ShouldLocationBarForXXX. Performs a navigation and then checks that
// the location bar visibility is as expcted.
void NavigateAndCheckForLocationBar(Browser* browser,
                                    const std::string& url_string,
                                    bool expected_visibility) {
  GURL url(url_string);
  NavigateToURLAndWait(browser, url);
  EXPECT_EQ(expected_visibility,
      browser->hosted_app_controller()->ShouldShowLocationBar());
}

}  // namespace

class HostedAppTest : public ExtensionBrowserTest,
                      public ::testing::WithParamInterface<bool> {
 public:
  HostedAppTest() : app_browser_(nullptr) {}
  ~HostedAppTest() override {}

  void SetUp() override {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(features::kDesktopPWAWindowing);
    } else {
#if defined(OS_MACOSX)
      scoped_feature_list_.InitAndEnableFeature(features::kBookmarkApps);
#endif
    }

    ExtensionBrowserTest::SetUp();
  }

 protected:
  void SetupApp(const std::string& app_folder, bool is_bookmark_app) {
    SetupApp(test_data_dir_.AppendASCII(app_folder), is_bookmark_app);
  }

  void SetupApp(const base::FilePath& app_folder, bool is_bookmark_app) {
    const Extension* app = InstallExtensionWithSourceAndFlags(
        app_folder, 1, extensions::Manifest::INTERNAL,
        is_bookmark_app ? extensions::Extension::FROM_BOOKMARK
                        : extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(app);

    // Launch it in a window.
    app_browser_ = LaunchAppBrowser(app);
    ASSERT_TRUE(app_browser_);
    ASSERT_TRUE(app_browser_ != browser());
  }

  void SetUpOnMainThread() override {
    ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  // Tests that performing |action| results in a new foreground tab
  // that navigated to |target_url| in the main browser window.
  void TestAppActionOpensForegroundTab(base::OnceClosure action,
                                       const GURL& target_url) {
    ASSERT_EQ(app_browser_, chrome::FindLastActive());

    size_t num_browsers = chrome::GetBrowserCount(profile());
    int num_tabs = browser()->tab_strip_model()->count();
    content::WebContents* initial_tab =
        browser()->tab_strip_model()->GetActiveWebContents();

    ASSERT_NO_FATAL_FAILURE(std::move(action).Run());

    EXPECT_EQ(num_browsers, chrome::GetBrowserCount(profile()));
    EXPECT_EQ(browser(), chrome::FindLastActive());
    EXPECT_EQ(++num_tabs, browser()->tab_strip_model()->count());

    content::WebContents* new_tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_NE(initial_tab, new_tab);
    EXPECT_EQ(target_url, new_tab->GetLastCommittedURL());
  }

  Browser* app_browser_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(HostedAppTest);
};

// Tests that "Open link in new tab" opens a link in a foreground tab.
IN_PROC_BROWSER_TEST_P(HostedAppTest, OpenLinkInNewTab) {
  SetupApp("app", true);

  const GURL url("http://www.foo.com/");
  TestAppActionOpensForegroundTab(
      base::BindOnce(
          [](content::WebContents* app_contents, const GURL& target_url) {
            ui_test_utils::UrlLoadObserver url_observer(
                target_url, content::NotificationService::AllSources());
            content::ContextMenuParams params;
            params.page_url = app_contents->GetLastCommittedURL();
            params.link_url = target_url;

            TestRenderViewContextMenu menu(app_contents->GetMainFrame(),
                                           params);
            menu.Init();
            menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
                                0 /* event_flags */);
            url_observer.Wait();
          },
          app_browser_->tab_strip_model()->GetActiveWebContents(), url),
      url);
}

// Tests that Ctrl + Clicking a link opens a foreground tab.
IN_PROC_BROWSER_TEST_P(HostedAppTest, CtrlClickLink) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Set up an app which covers app.com URLs.
  GURL app_url =
      embedded_test_server()->GetURL("app.com", "/click_modifier/href.html");
  ui_test_utils::UrlLoadObserver url_observer(
      app_url, content::NotificationService::AllSources());
  extensions::TestExtensionDir test_app_dir;
  test_app_dir.WriteManifest(
      base::StringPrintf(kAppDotComManifest, app_url.spec().c_str()));
  SetupApp(test_app_dir.UnpackedPath(), false);
  // Wait for the URL to load so that we can click on the page.
  url_observer.Wait();

  const GURL url = embedded_test_server()->GetURL(
      "app.com", "/click_modifier/new_window.html");
  TestAppActionOpensForegroundTab(
      base::BindOnce(
          [](content::WebContents* app_contents, const GURL& target_url) {
            ui_test_utils::UrlLoadObserver url_observer(
                target_url, content::NotificationService::AllSources());
            int ctrl_key;
#if defined(OS_MACOSX)
            ctrl_key = blink::WebInputEvent::Modifiers::kMetaKey;
#else
            ctrl_key = blink::WebInputEvent::Modifiers::kControlKey;
#endif
            content::SimulateMouseClick(app_contents, ctrl_key,
                                        blink::WebMouseEvent::Button::kLeft);
            url_observer.Wait();
          },
          app_browser_->tab_strip_model()->GetActiveWebContents(), url),
      url);
}

// Check that the location bar is shown correctly for bookmark apps.
IN_PROC_BROWSER_TEST_P(HostedAppTest, ShouldShowLocationBarForBookmarkApp) {
  SetupApp("app", true);

  // Navigate to the app's launch page; the location bar should be hidden.
  NavigateAndCheckForLocationBar(
      app_browser_, "http://www.example.com/empty.html", false);

  // Navigate to another page on the same origin; the location bar should still
  // hidden.
  NavigateAndCheckForLocationBar(
      app_browser_, "http://www.example.com/blah", false);

  // Navigate to different origin; the location bar should now be visible.
  NavigateAndCheckForLocationBar(
      app_browser_, "http://www.foo.com/blah", true);
}

// Check that the location bar is shown correctly for HTTP bookmark apps when
// they navigate to a HTTPS page on the same origin.
IN_PROC_BROWSER_TEST_P(HostedAppTest, ShouldShowLocationBarForHTTPBookmarkApp) {
  SetupApp("app", true);

  // Navigate to the app's launch page; the location bar should be hidden.
  NavigateAndCheckForLocationBar(
      app_browser_, "http://www.example.com/empty.html", false);

  // Navigate to the https version of the site; the location bar should
  // be hidden.
  NavigateAndCheckForLocationBar(
      app_browser_, "https://www.example.com/blah", false);
}

// Check that the location bar is shown correctly for HTTPS bookmark apps when
// they navigate to a HTTP page on the same origin.
IN_PROC_BROWSER_TEST_P(HostedAppTest,
                       ShouldShowLocationBarForHTTPSBookmarkApp) {
  SetupApp("https_app", true);

  // Navigate to the app's launch page; the location bar should be hidden.
  NavigateAndCheckForLocationBar(
      app_browser_, "https://www.example.com/empty.html", false);

  // Navigate to the http version of the site; the location bar should
  // be visible for the https version as it is now on a less secure version
  // of its host.
  NavigateAndCheckForLocationBar(
      app_browser_, "http://www.example.com/blah", true);
}

// Check that the location bar is shown correctly for normal hosted apps.
IN_PROC_BROWSER_TEST_P(HostedAppTest, ShouldShowLocationBarForHostedApp) {
  SetupApp("app", false);

  // Navigate to the app's launch page; the location bar should be hidden.
  NavigateAndCheckForLocationBar(
      app_browser_, "http://www.example.com/empty.html", false);

  // Navigate to another page on the same origin; the location bar should still
  // hidden.
  NavigateAndCheckForLocationBar(
      app_browser_, "http://www.example.com/blah", false);

  // Navigate to different origin; the location bar should now be visible.
  NavigateAndCheckForLocationBar(
      app_browser_, "http://www.foo.com/blah", true);
}

// Check that the location bar is shown correctly for hosted apps that specify
// start URLs without the 'www.' prefix.
IN_PROC_BROWSER_TEST_P(HostedAppTest, LocationBarForHostedAppWithoutWWW) {
  SetupApp("app_no_www", false);

  // Navigate to the app's launch page; the location bar should be hidden.
  NavigateAndCheckForLocationBar(
      app_browser_, "http://example.com/empty.html", false);

  // Navigate to the app's launch page with the 'www.' prefis; the location bar
  // should be hidden.
  NavigateAndCheckForLocationBar(
      app_browser_, "http://www.example.com/empty.html", false);

  // Navigate to different origin; the location bar should now be visible.
  NavigateAndCheckForLocationBar(
      app_browser_, "http://www.foo.com/blah", true);
}

// Check that a subframe on a regular web page can navigate to a URL that
// redirects to a hosted app.  https://crbug.com/721949.
IN_PROC_BROWSER_TEST_P(HostedAppTest, SubframeRedirectsToHostedApp) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Set up an app which covers app.com URLs.
  GURL app_url = embedded_test_server()->GetURL("app.com", "/title1.html");
  extensions::TestExtensionDir test_app_dir;
  test_app_dir.WriteManifest(
      base::StringPrintf(kAppDotComManifest, app_url.spec().c_str()));
  SetupApp(test_app_dir.UnpackedPath(), false);

  // Navigate a regular tab to a page with a subframe.
  GURL url = embedded_test_server()->GetURL("foo.com", "/iframe.html");
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigateToURLAndWait(browser(), url);

  // Navigate the subframe to a URL that redirects to a URL in the hosted app's
  // web extent.
  GURL redirect_url = embedded_test_server()->GetURL(
      "bar.com", "/server-redirect?" + app_url.spec());
  EXPECT_TRUE(NavigateIframeToURL(tab, "test", redirect_url));

  // Ensure that the frame navigated successfully and that it has correct
  // content.
  RenderFrameHost* subframe = content::ChildFrameAt(tab->GetMainFrame(), 0);
  EXPECT_EQ(app_url, subframe->GetLastCommittedURL());
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      subframe, "window.domAutomationController.send(document.body.innerText);",
      &result));
  EXPECT_EQ("This page has no title.", result);
}

IN_PROC_BROWSER_TEST_P(HostedAppTest, BookmarkAppThemeColor) {
  {
    WebApplicationInfo web_app_info;
    web_app_info.app_url = GURL(kExampleURL);
    web_app_info.scope = GURL(kExampleURL);
    web_app_info.theme_color = SkColorSetA(SK_ColorBLUE, 0xF0);
    const extensions::Extension* app = InstallBookmarkApp(web_app_info);
    Browser* app_browser = LaunchAppBrowser(app);

    EXPECT_EQ(
        web_app::GetExtensionIdFromApplicationName(app_browser->app_name()),
        app->id());
    EXPECT_EQ(web_app_info.theme_color,
              app_browser->hosted_app_controller()->GetThemeColor().value());
  }
  {
    WebApplicationInfo web_app_info;
    web_app_info.app_url = GURL("http://example.org/2");
    web_app_info.scope = GURL("http://example.org/");
    web_app_info.theme_color = base::Optional<SkColor>();
    const extensions::Extension* app = InstallBookmarkApp(web_app_info);
    Browser* app_browser = LaunchAppBrowser(app);

    EXPECT_EQ(
        web_app::GetExtensionIdFromApplicationName(app_browser->app_name()),
        app->id());
    EXPECT_FALSE(
        app_browser->hosted_app_controller()->GetThemeColor().has_value());
  }
}

// Ensure that hosted app windows with blank titles don't display the URL as a
// default window title.
IN_PROC_BROWSER_TEST_P(HostedAppTest, Title) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("app.site.com", "/empty.html");
  WebApplicationInfo web_app_info;
  web_app_info.app_url = url;
  const extensions::Extension* app = InstallBookmarkApp(web_app_info);

  Browser* app_browser = LaunchAppBrowser(app);
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(web_contents);
  EXPECT_EQ(base::string16(), app_browser->GetWindowTitleForCurrentTab(false));
  NavigateToURLAndWait(app_browser, embedded_test_server()->GetURL(
                                        "app.site.com", "/simple.html"));
  EXPECT_EQ(base::ASCIIToUTF16("OK"),
            app_browser->GetWindowTitleForCurrentTab(false));
}

using HostedAppPWAOnlyTest = HostedAppTest;

// Check the 'Copy URL' menu button for Hosted App windows.
IN_PROC_BROWSER_TEST_P(HostedAppPWAOnlyTest, CopyURL) {
  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL(kExampleURL);
  const extensions::Extension* app = InstallBookmarkApp(web_app_info);
  Browser* app_browser = LaunchAppBrowser(app);

  content::BrowserTestClipboardScope test_clipboard_scope;
  chrome::ExecuteCommand(app_browser, IDC_COPY_URL);

  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  base::string16 result;
  clipboard->ReadText(ui::CLIPBOARD_TYPE_COPY_PASTE, &result);
  EXPECT_EQ(result, base::UTF8ToUTF16(kExampleURL));
}

// Check the 'Open in Chrome' menu button for Hosted App windows.
IN_PROC_BROWSER_TEST_P(HostedAppPWAOnlyTest, OpenInChrome) {
  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL(kExampleURL);
  const extensions::Extension* app = InstallBookmarkApp(web_app_info);
  {
    Browser* app_browser = LaunchAppBrowser(app);

    EXPECT_EQ(1, app_browser->tab_strip_model()->count());
    EXPECT_EQ(1, browser()->tab_strip_model()->count());
    ASSERT_EQ(2u, chrome::GetBrowserCount(browser()->profile()));

    chrome::ExecuteCommand(app_browser, IDC_OPEN_IN_CHROME);

    // The browser frame is closed next event loop so it's still safe to access
    // here.
    EXPECT_EQ(0, app_browser->tab_strip_model()->count());

    EXPECT_EQ(2, browser()->tab_strip_model()->count());
    EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
    EXPECT_EQ(
        GURL(kExampleURL),
        browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
  }

  // Wait until the browser actually gets closed. This invalidates
  // |app_browser|.
  content::RunAllPendingInMessageLoop();
  ASSERT_EQ(1u, chrome::GetBrowserCount(browser()->profile()));
}

// Check the 'App info' menu button for Hosted App windows.
IN_PROC_BROWSER_TEST_P(HostedAppPWAOnlyTest, AppInfo) {
  WebApplicationInfo web_app_info;
  web_app_info.app_url = GURL(kExampleURL);
  const extensions::Extension* app = InstallBookmarkApp(web_app_info);
  Browser* app_browser = LaunchAppBrowser(app);

  bool dialog_created = false;

  GetAppInfoDialogCreatedCallbackForTesting() = base::BindOnce(
      [](bool* dialog_created) { *dialog_created = true; }, &dialog_created);

  chrome::ExecuteCommand(app_browser, IDC_APP_INFO);

  EXPECT_TRUE(dialog_created);

  // The test closure should have run. But clear the global in case it hasn't.
  EXPECT_FALSE(GetAppInfoDialogCreatedCallbackForTesting());
  GetAppInfoDialogCreatedCallbackForTesting().Reset();
}

// Common app manifest for HostedAppProcessModelTests.
constexpr const char kHostedAppProcessModelManifest[] =
    R"( { "name": "Hosted App Process Model Test",
            "version": "1",
            "manifest_version": 2,
            "app": {
              "launch": {
                "web_url": "%s"
              },
              "urls": ["*://app.site.com/frame_tree",  "*://isolated.site.com/"]
            }
          } )";

// This set of tests verifies the hosted app process model behavior in various
// isolation modes. They can be run by default, with --site-per-process, or
// with --top-document-isolation. In each mode, they contain an isolated origin.
//
// Relevant frames in the tests:
// - |app| - app.site.com/frame_tree/cross_origin_but_same_site_frames.html
//           Main frame, launch URL of the hosted app (i.e. app.launch.web_url).
// - |same_dir| - app.site.com/frame_tree/simple.htm
//                Another URL, but still covered by hosted app's web extent
//                (i.e. by app.urls).
// - |diff_dir| - app.site.com/save_page/a.htm
//                Same origin as |same_dir| and |app|, but not covered by app's
//                extent.
// - |same_site| - other.site.com/title1.htm
//                 Different origin, but same site as |app|, |same_dir|,
//                 |diff_dir|.
// - |isolated| - isolated.site.com/title1.htm
//                Within app's extent, but belongs to an isolated origin.
// - |cross_site| - cross.domain.com/title1.htm
//                  Cross-site from all the other frames.

class HostedAppProcessModelTest : public HostedAppTest {
 public:
  HostedAppProcessModelTest() {}
  ~HostedAppProcessModelTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    std::string origin_list =
        embedded_test_server()->GetURL("isolated.site.com", "/").spec();
    command_line->AppendSwitchASCII(switches::kIsolateOrigins, origin_list);
  }

  void SetUpOnMainThread() override {
    HostedAppTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->StartAcceptingConnections();

    // TODO(alexmos): This should also be true for TDI, if
    // base::FeatureList::IsEnabled(features::kTopDocumentIsolation) is true.
    // However, we can't do that yet, because
    // ChromeContentBrowserClientExtensionsPart::
    // ShouldFrameShareParentSiteInstanceDespiteTopDocumentIsolation() returns
    // true for all hosted apps and hence forces even cross-site subframes to
    // be kept in the app process.
    should_swap_for_cross_site_ = content::AreAllSitesIsolatedForTesting();

    process_map_ = extensions::ProcessMap::Get(browser()->profile());

    same_dir_url_ = embedded_test_server()->GetURL("app.site.com",
                                                   "/frame_tree/simple.htm");
    diff_dir_url_ =
        embedded_test_server()->GetURL("app.site.com", "/save_page/a.htm");
    same_site_url_ =
        embedded_test_server()->GetURL("other.site.com", "/title1.html");
    isolated_url_ =
        embedded_test_server()->GetURL("isolated.site.com", "/title1.html");
    cross_site_url_ =
        embedded_test_server()->GetURL("cross.domain.com", "/title1.html");
  }

  // Opens a popup from |rfh| to |url|, verifies whether it should stay in the
  // same process as |rfh| and whether it should be in an app process, and then
  // closes the popup.
  void TestPopupProcess(RenderFrameHost* rfh,
                        const GURL& url,
                        bool expect_same_process,
                        bool expect_app_process) {
    content::WebContentsAddedObserver tab_added_observer;
    ASSERT_TRUE(
        content::ExecuteScript(rfh, "window.open('" + url.spec() + "');"));
    content::WebContents* new_tab = tab_added_observer.GetWebContents();
    ASSERT_TRUE(new_tab);
    EXPECT_TRUE(WaitForLoadStop(new_tab));
    EXPECT_EQ(url, new_tab->GetLastCommittedURL());
    RenderFrameHost* new_rfh = new_tab->GetMainFrame();

    EXPECT_EQ(expect_same_process, rfh->GetProcess() == new_rfh->GetProcess())
        << " for " << url << " from " << rfh->GetLastCommittedURL();

    EXPECT_EQ(expect_app_process,
              process_map_->Contains(new_rfh->GetProcess()->GetID()))
        << " for " << url << " from " << rfh->GetLastCommittedURL();
    EXPECT_EQ(expect_app_process,
              new_rfh->GetSiteInstance()->GetSiteURL().SchemeIs(
                  extensions::kExtensionScheme))
        << " for " << url << " from " << rfh->GetLastCommittedURL();

    ASSERT_TRUE(content::ExecuteScript(new_rfh, "window.close();"));
  }

  // Creates a subframe underneath |parent_rfh| to |url|, verifies whether it
  // should stay in the same process as |parent_rfh| and whether it should be in
  // an app process, and returns the subframe RFH.
  RenderFrameHost* TestSubframeProcess(RenderFrameHost* parent_rfh,
                                       const GURL& url,
                                       bool expect_same_process,
                                       bool expect_app_process) {
    return TestSubframeProcess(parent_rfh, url, "", expect_same_process,
                               expect_app_process);
  }

  RenderFrameHost* TestSubframeProcess(RenderFrameHost* parent_rfh,
                                       const GURL& url,
                                       const std::string& element_id,
                                       bool expect_same_process,
                                       bool expect_app_process) {
    WebContents* web_contents = WebContents::FromRenderFrameHost(parent_rfh);
    content::TestNavigationObserver nav_observer(web_contents, 1);
    std::string script = "var f = document.createElement('iframe');";
    if (!element_id.empty())
      script += "f.id = '" + element_id + "';";
    script += "f.src = '" + url.spec() + "';";
    script += "document.body.appendChild(f);";
    EXPECT_TRUE(ExecuteScript(parent_rfh, script));
    nav_observer.Wait();

    RenderFrameHost* subframe = content::FrameMatchingPredicate(
        web_contents, base::Bind(&content::FrameHasSourceUrl, url));

    EXPECT_EQ(expect_same_process,
              parent_rfh->GetProcess() == subframe->GetProcess())
        << " for " << url << " from " << parent_rfh->GetLastCommittedURL();

    EXPECT_EQ(expect_app_process,
              process_map_->Contains(subframe->GetProcess()->GetID()))
        << " for " << url << " from " << parent_rfh->GetLastCommittedURL();
    EXPECT_EQ(expect_app_process,
              subframe->GetSiteInstance()->GetSiteURL().SchemeIs(
                  extensions::kExtensionScheme))
        << " for " << url << " from " << parent_rfh->GetLastCommittedURL();

    return subframe;
  }

 protected:
  bool should_swap_for_cross_site_;

  extensions::ProcessMap* process_map_;

  GURL same_dir_url_;
  GURL diff_dir_url_;
  GURL same_site_url_;
  GURL isolated_url_;
  GURL cross_site_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostedAppProcessModelTest);
};

// Tests that same-site iframes stay inside the hosted app process, even when
// they are not within the hosted app's extent.  This allows same-site scripting
// to work and avoids unnecessary OOPIFs.  Also tests that isolated origins in
// iframes do not stay in the app's process, nor do cross-site iframes in modes
// that require them to swap.
IN_PROC_BROWSER_TEST_P(HostedAppProcessModelTest, IframesInsideHostedApp) {
  // Set up and launch the hosted app.
  GURL url = embedded_test_server()->GetURL(
      "app.site.com", "/frame_tree/cross_origin_but_same_site_frames.html");

  extensions::TestExtensionDir test_app_dir;
  test_app_dir.WriteManifest(
      base::StringPrintf(kHostedAppProcessModelManifest, url.spec().c_str()));
  SetupApp(test_app_dir.UnpackedPath(), false);

  content::WebContents* web_contents =
      app_browser_->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  auto find_frame = [web_contents](const std::string& name) {
    return content::FrameMatchingPredicate(
        web_contents, base::Bind(&content::FrameMatchesName, name));
  };
  RenderFrameHost* app = web_contents->GetMainFrame();
  RenderFrameHost* same_dir = find_frame("SameOrigin-SamePath");
  RenderFrameHost* diff_dir = find_frame("SameOrigin-DifferentPath");
  RenderFrameHost* same_site = find_frame("OtherSubdomain-SameSite");
  RenderFrameHost* isolated = find_frame("Isolated-SameSite");
  RenderFrameHost* cross_site = find_frame("CrossSite");

  // Sanity-check sites of all relevant frames to verify test setup.
  GURL app_site = content::SiteInstance::GetSiteForURL(
      app_browser_->profile(), app->GetLastCommittedURL());
  EXPECT_EQ(extensions::kExtensionScheme, app_site.scheme());

  GURL same_dir_site = content::SiteInstance::GetSiteForURL(
      app_browser_->profile(), same_dir->GetLastCommittedURL());
  EXPECT_EQ(extensions::kExtensionScheme, same_dir_site.scheme());
  EXPECT_EQ(same_dir_site, app_site);

  GURL diff_dir_site = content::SiteInstance::GetSiteForURL(
      app_browser_->profile(), diff_dir->GetLastCommittedURL());
  EXPECT_NE(extensions::kExtensionScheme, diff_dir_site.scheme());
  EXPECT_NE(diff_dir_site, app_site);

  GURL same_site_site = content::SiteInstance::GetSiteForURL(
      app_browser_->profile(), same_site->GetLastCommittedURL());
  EXPECT_NE(extensions::kExtensionScheme, same_site_site.scheme());
  EXPECT_NE(same_site_site, app_site);
  EXPECT_EQ(same_site_site, diff_dir_site);

  GURL isolated_site = content::SiteInstance::GetSiteForURL(
      app_browser_->profile(), isolated->GetLastCommittedURL());
  EXPECT_NE(extensions::kExtensionScheme, isolated_site.scheme());
  EXPECT_NE(isolated_site, app_site);
  EXPECT_NE(isolated_site, diff_dir_site);

  GURL cross_site_site = content::SiteInstance::GetSiteForURL(
      app_browser_->profile(), cross_site->GetLastCommittedURL());
  EXPECT_NE(cross_site_site, app_site);
  EXPECT_NE(cross_site_site, same_site_site);

  // Verify that |same_dir| and |diff_dir| have the same origin according to
  // |window.origin| (even though they have different |same_dir_site| and
  // |diff_dir_site|).
  std::string same_dir_origin;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      same_dir, "domAutomationController.send(window.origin)",
      &same_dir_origin));
  std::string diff_dir_origin;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      diff_dir, "domAutomationController.send(window.origin)",
      &diff_dir_origin));
  EXPECT_EQ(diff_dir_origin, same_dir_origin);

  // Verify that (1) all same-site iframes stay in the process, (2) isolated
  // origin iframe does not, and (3) cross-site iframe leaves if the process
  // model calls for it.
  EXPECT_EQ(same_dir->GetProcess(), app->GetProcess());
  EXPECT_EQ(diff_dir->GetProcess(), app->GetProcess());
  EXPECT_EQ(same_site->GetProcess(), app->GetProcess());
  EXPECT_NE(isolated->GetProcess(), app->GetProcess());
  if (should_swap_for_cross_site_)
    EXPECT_NE(cross_site->GetProcess(), app->GetProcess());
  else
    EXPECT_EQ(cross_site->GetProcess(), app->GetProcess());

  // The isolated origin iframe's process should not be in the ProcessMap. If
  // we swapped processes for the |cross_site| iframe, its process should also
  // not be on the ProcessMap.
  EXPECT_FALSE(process_map_->Contains(isolated->GetProcess()->GetID()));
  if (should_swap_for_cross_site_)
    EXPECT_FALSE(process_map_->Contains(cross_site->GetProcess()->GetID()));

  // Verify that |same_dir| and |diff_dir| can script each other.
  // (they should - they have the same origin).
  std::string inner_text_from_other_frame;
  const std::string r_script =
      R"( var w = window.open('', 'SameOrigin-SamePath');
          domAutomationController.send(w.document.body.innerText); )";
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      diff_dir, r_script, &inner_text_from_other_frame));
  EXPECT_EQ("Simple test page.", inner_text_from_other_frame);
}

// Check that if a hosted app has an iframe, and that iframe navigates to URLs
// that are same-site with the app, these navigations ends up in the app
// process.
IN_PROC_BROWSER_TEST_P(HostedAppProcessModelTest,
                       IframeNavigationsInsideHostedApp) {
  // Set up and launch the hosted app.
  GURL app_url =
      embedded_test_server()->GetURL("app.site.com", "/frame_tree/simple.htm");

  extensions::TestExtensionDir test_app_dir;
  test_app_dir.WriteManifest(base::StringPrintf(kHostedAppProcessModelManifest,
                                                app_url.spec().c_str()));
  SetupApp(test_app_dir.UnpackedPath(), false);

  content::WebContents* web_contents =
      app_browser_->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  RenderFrameHost* app = web_contents->GetMainFrame();

  // Add a data: URL subframe.  This should stay in the app process.
  TestSubframeProcess(app, GURL("data:text/html,foo"), "test_iframe",
                      true /* expect_same_process */,
                      true /* expect_app_process */);

  // Navigate iframe to a non-app-but-same-site-with-app URL and check that it
  // stays in the parent process.
  {
    SCOPED_TRACE("... for data: -> diff_dir");
    EXPECT_TRUE(
        NavigateIframeToURL(web_contents, "test_iframe", diff_dir_url_));
    EXPECT_EQ(ChildFrameAt(app, 0)->GetProcess(), app->GetProcess());
  }

  // Navigate the iframe to an isolated origin to force an OOPIF.
  {
    SCOPED_TRACE("... for diff_dir -> isolated");
    EXPECT_TRUE(
        NavigateIframeToURL(web_contents, "test_iframe", isolated_url_));
    EXPECT_NE(ChildFrameAt(app, 0)->GetProcess(), app->GetProcess());
  }

  // Navigate the iframe to an app URL. This should go back to the app process.
  {
    SCOPED_TRACE("... for isolated -> same_dir");
    EXPECT_TRUE(
        NavigateIframeToURL(web_contents, "test_iframe", same_dir_url_));
    EXPECT_EQ(ChildFrameAt(app, 0)->GetProcess(), app->GetProcess());
  }

  // Navigate the iframe back to the OOPIF again.
  {
    SCOPED_TRACE("... for same_dir -> isolated");
    EXPECT_TRUE(
        NavigateIframeToURL(web_contents, "test_iframe", isolated_url_));
    EXPECT_NE(ChildFrameAt(app, 0)->GetProcess(), app->GetProcess());
  }

  // Navigate iframe to a non-app-but-same-site-with-app URL and check that it
  // also goes back to the parent process.
  {
    SCOPED_TRACE("... for isolated -> diff_dir");
    EXPECT_TRUE(
        NavigateIframeToURL(web_contents, "test_iframe", diff_dir_url_));
    EXPECT_EQ(ChildFrameAt(app, 0)->GetProcess(), app->GetProcess());
  }
}

// Tests that popups opened within a hosted app behave as expected.
// TODO(creis): The goal is for same-site popups to stay in the app's process so
// that they can be scripted, but this is not yet supported.  See
// https://crbug.com/718516.
IN_PROC_BROWSER_TEST_P(HostedAppProcessModelTest, PopupsInsideHostedApp) {
  // Set up and launch the hosted app.
  GURL url = embedded_test_server()->GetURL(
      "app.site.com", "/frame_tree/cross_origin_but_same_site_frames.html");

  extensions::TestExtensionDir test_app_dir;
  test_app_dir.WriteManifest(
      base::StringPrintf(kHostedAppProcessModelManifest, url.spec().c_str()));
  SetupApp(test_app_dir.UnpackedPath(), false);

  content::WebContents* web_contents =
      app_browser_->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  auto find_frame = [web_contents](const std::string& name) {
    return content::FrameMatchingPredicate(
        web_contents, base::Bind(&content::FrameMatchesName, name));
  };
  RenderFrameHost* app = web_contents->GetMainFrame();
  RenderFrameHost* same_dir = find_frame("SameOrigin-SamePath");
  RenderFrameHost* diff_dir = find_frame("SameOrigin-DifferentPath");
  RenderFrameHost* same_site = find_frame("OtherSubdomain-SameSite");
  RenderFrameHost* isolated = find_frame("Isolated-SameSite");
  RenderFrameHost* cross_site = find_frame("CrossSite");

  {
    SCOPED_TRACE("... for same_dir popup");
    TestPopupProcess(app, same_dir_url_, true, true);
  }
  {
    SCOPED_TRACE("... for diff_dir popup");
    // TODO(creis): This should stay in the app's process, but that's not yet
    // supported in modes that swap for cross-site navigations.
    TestPopupProcess(app, diff_dir_url_, !should_swap_for_cross_site_,
                     !should_swap_for_cross_site_);
  }
  {
    SCOPED_TRACE("... for same_site popup");
    // TODO(creis): This should stay in the app's process, but that's not yet
    // supported in modes that swap for cross-site navigations.
    TestPopupProcess(app, same_site_url_, !should_swap_for_cross_site_,
                     !should_swap_for_cross_site_);
  }
  {
    SCOPED_TRACE("... for isolated_url popup");
    TestPopupProcess(app, isolated_url_, false, false);
  }
  // For cross-site, the resulting process is only in the app process if we
  // don't swap processes.
  {
    SCOPED_TRACE("... for cross_site popup");
    TestPopupProcess(app, cross_site_url_, !should_swap_for_cross_site_,
                     !should_swap_for_cross_site_);
  }

  // If the iframes open popups that are same-origin with themselves, the popups
  // should be in the same process as the respective iframes.
  {
    SCOPED_TRACE("... for same_dir iframe popup");
    TestPopupProcess(same_dir, same_dir_url_, true, true);
  }
  {
    SCOPED_TRACE("... for diff_dir iframe popup");
    // TODO(creis): This should stay in the app's process, but that's not yet
    // supported in modes that swap for cross-site navigations.
    TestPopupProcess(diff_dir, diff_dir_url_, !should_swap_for_cross_site_,
                     !should_swap_for_cross_site_);
  }
  {
    SCOPED_TRACE("... for same_site iframe popup");
    // TODO(creis): This should stay in the app's process, but that's not yet
    // supported in modes that swap for cross-site navigations.
    TestPopupProcess(same_site, same_site_url_, !should_swap_for_cross_site_,
                     !should_swap_for_cross_site_);
  }
  {
    SCOPED_TRACE("... for isolated_url iframe popup");
    TestPopupProcess(isolated, isolated_url_, true, false);
  }
  {
    SCOPED_TRACE("... for cross_site iframe popup");
    TestPopupProcess(cross_site, cross_site_url_, true,
                     !should_swap_for_cross_site_);
  }
}

// Tests that hosted app URLs loaded in iframes of non-app pages won't cause an
// OOPIF unless there is another reason to create it, but popups from outside
// the app will swap into the app.
IN_PROC_BROWSER_TEST_P(HostedAppProcessModelTest, FromOutsideHostedApp) {
  // Set up and launch the hosted app.
  GURL app_url =
      embedded_test_server()->GetURL("app.site.com", "/frame_tree/simple.htm");

  extensions::TestExtensionDir test_app_dir;
  test_app_dir.WriteManifest(base::StringPrintf(kHostedAppProcessModelManifest,
                                                app_url.spec().c_str()));
  SetupApp(test_app_dir.UnpackedPath(), false);

  content::WebContents* web_contents =
      app_browser_->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  // Starting same-origin but outside the app, popups should swap to the app.
  {
    SCOPED_TRACE("... from diff_dir");
    ui_test_utils::NavigateToURL(app_browser_, diff_dir_url_);
    RenderFrameHost* main_frame = web_contents->GetMainFrame();
    EXPECT_FALSE(main_frame->GetSiteInstance()->GetSiteURL().SchemeIs(
        extensions::kExtensionScheme));
    TestPopupProcess(main_frame, app_url, false, true);
    // Subframes in the app should not swap.
    RenderFrameHost* diff_dir_rfh =
        TestSubframeProcess(main_frame, app_url, true, false);
    // Popups from the subframe, though same-origin, should swap to the app.
    // See https://crbug.com/89272.
    TestPopupProcess(diff_dir_rfh, app_url, false, true);
  }

  // Starting same-site but outside the app, popups should swap to the app.
  {
    SCOPED_TRACE("... from same_site");
    ui_test_utils::NavigateToURL(app_browser_, same_site_url_);
    RenderFrameHost* main_frame = web_contents->GetMainFrame();
    EXPECT_FALSE(main_frame->GetSiteInstance()->GetSiteURL().SchemeIs(
        extensions::kExtensionScheme));
    TestPopupProcess(main_frame, app_url, false, true);
    // Subframes in the app should not swap.
    RenderFrameHost* same_site_rfh =
        TestSubframeProcess(main_frame, app_url, true, false);
    // Popups from the subframe should swap to the app, as above.
    TestPopupProcess(same_site_rfh, app_url, false, true);
  }

  // Starting on an isolated origin, popups should swap to the app.
  {
    SCOPED_TRACE("... from isolated_url");
    ui_test_utils::NavigateToURL(app_browser_, isolated_url_);
    RenderFrameHost* main_frame = web_contents->GetMainFrame();
    EXPECT_FALSE(main_frame->GetSiteInstance()->GetSiteURL().SchemeIs(
        extensions::kExtensionScheme));
    TestPopupProcess(main_frame, app_url, false, true);
    // Subframes in the app should swap process.
    // TODO(creis): Perhaps this OOPIF should not be an app process?
    RenderFrameHost* isolated_rfh =
        TestSubframeProcess(main_frame, app_url, false, true);
    // Popups from the subframe into the app should be in the app process.
    TestPopupProcess(isolated_rfh, app_url, true, true);
  }

  // Starting cross-site, popups should swap to the app.
  {
    SCOPED_TRACE("... from cross_site");
    ui_test_utils::NavigateToURL(app_browser_, cross_site_url_);
    RenderFrameHost* main_frame = web_contents->GetMainFrame();
    EXPECT_FALSE(main_frame->GetSiteInstance()->GetSiteURL().SchemeIs(
        extensions::kExtensionScheme));
    TestPopupProcess(main_frame, app_url, false, true);
    // Subframes in the app should swap if the process model needs it.
    // TODO(creis): Perhaps this OOPIF should not be an app process?
    RenderFrameHost* cross_site_rfh =
        TestSubframeProcess(main_frame, app_url, !should_swap_for_cross_site_,
                            should_swap_for_cross_site_);
    // Popups from the subframe into the app should be in the app process.
    TestPopupProcess(cross_site_rfh, app_url, should_swap_for_cross_site_,
                     true);
  }
}

INSTANTIATE_TEST_CASE_P(/* no prefix */, HostedAppTest, ::testing::Bool());
INSTANTIATE_TEST_CASE_P(/* no prefix */,
                        HostedAppPWAOnlyTest,
                        ::testing::Values(true));
INSTANTIATE_TEST_CASE_P(/* no prefix */,
                        HostedAppProcessModelTest,
                        ::testing::Bool());
