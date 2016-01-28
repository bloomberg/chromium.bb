// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"

using content::WebContents;
using extensions::Extension;

namespace {

// Used by ShouldLocationBarForXXX. Performs a navigation and then checks that
// the location bar visibility is as expcted.
void NavigateAndCheckForLocationBar(Browser* browser,
                                    const std::string& url_string,
                                    bool expected_visibility) {
  GURL url(url_string);
  ui_test_utils::NavigateToURL(browser, url);
  EXPECT_EQ(expected_visibility,
      browser->hosted_app_controller()->ShouldShowLocationBar());
}

}  // namespace

class HostedAppTest : public ExtensionBrowserTest {
 public:
  HostedAppTest() : app_browser_(nullptr) {}
  ~HostedAppTest() override {}

 protected:
  void SetupApp(const std::string& app_folder, bool is_bookmark_app) {
    const Extension* app = InstallExtensionWithSourceAndFlags(
        test_data_dir_.AppendASCII(app_folder), 1,
        extensions::Manifest::INTERNAL,
        is_bookmark_app ? extensions::Extension::FROM_BOOKMARK
                        : extensions::Extension::NO_FLAGS);
    ASSERT_TRUE(app);

    // Launch it in a window.
    ASSERT_TRUE(OpenApplication(AppLaunchParams(
        browser()->profile(), app, extensions::LAUNCH_CONTAINER_WINDOW,
        NEW_WINDOW, extensions::SOURCE_TEST)));

    for (auto* b : *BrowserList::GetInstance()) {
      if (b == browser())
        continue;

      std::string browser_app_id =
          web_app::GetExtensionIdFromApplicationName(b->app_name());
      if (browser_app_id == app->id()) {
        app_browser_ = b;
        break;
      }
    }

    ASSERT_TRUE(app_browser_);
    ASSERT_TRUE(app_browser_ != browser());
  }

  Browser* app_browser_;
};

// Check that the location bar is shown correctly for bookmark apps.
IN_PROC_BROWSER_TEST_F(HostedAppTest,
                       ShouldShowLocationBarForBookmarkApp) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableNewBookmarkApps);

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
IN_PROC_BROWSER_TEST_F(HostedAppTest,
                       ShouldShowLocationBarForHTTPBookmarkApp) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableNewBookmarkApps);

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
IN_PROC_BROWSER_TEST_F(HostedAppTest,
                       ShouldShowLocationBarForHTTPSBookmarkApp) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableNewBookmarkApps);

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
IN_PROC_BROWSER_TEST_F(HostedAppTest,
                       ShouldShowLocationBarForHostedApp) {
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
IN_PROC_BROWSER_TEST_F(HostedAppTest,
                       LocationBarForHostedAppWithoutWWW) {
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

// Open a normal browser window, a hosted app window, a legacy packaged app
// window and a dev tools window, and check that the web app frame feature is
// supported correctly.
IN_PROC_BROWSER_TEST_F(HostedAppTest, ShouldUseWebAppFrame) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableWebAppFrame);

  // Load a hosted app.
   const Extension* bookmark_app = InstallExtensionWithSourceAndFlags(
      test_data_dir_.AppendASCII("app"),
      1,
      extensions::Manifest::INTERNAL,
      extensions::Extension::FROM_BOOKMARK);
  ASSERT_TRUE(bookmark_app);

  // Launch it in a window, as AppLauncherHandler::HandleLaunchApp() would.
  WebContents* bookmark_app_window = OpenApplication(AppLaunchParams(
      browser()->profile(), bookmark_app, extensions::LAUNCH_CONTAINER_WINDOW,
      NEW_WINDOW, extensions::SOURCE_UNTRACKED));
  ASSERT_TRUE(bookmark_app_window);

  //  Load a packaged app.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("packaged_app")));
  const Extension* packaged_app = nullptr;
  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser()->profile());
  for (const scoped_refptr<const extensions::Extension>& extension :
       registry->enabled_extensions()) {
    if (extension->name() == "Packaged App Test")
      packaged_app = extension.get();
  }
  ASSERT_TRUE(packaged_app);

  // Launch it in a window, as AppLauncherHandler::HandleLaunchApp() would.
  WebContents* packaged_app_window = OpenApplication(AppLaunchParams(
      browser()->profile(), packaged_app, extensions::LAUNCH_CONTAINER_WINDOW,
      NEW_WINDOW, extensions::SOURCE_UNTRACKED));
  ASSERT_TRUE(packaged_app_window);

  DevToolsWindow* devtools_window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), false);

  // The launch should have created a new app browser and a dev tools browser.
  ASSERT_EQ(4u, chrome::GetBrowserCount(browser()->profile(),
                                        browser()->host_desktop_type()));

  // Find the new browsers.
  Browser* bookmark_app_browser = nullptr;
  Browser* packaged_app_browser = nullptr;
  Browser* dev_tools_browser = nullptr;
  for (auto* b : *BrowserList::GetInstance()) {
    if (b == browser()) {
      continue;
    } else if (b->app_name() == DevToolsWindow::kDevToolsApp) {
      dev_tools_browser = b;
    } else if (b->tab_strip_model()->GetActiveWebContents() ==
               bookmark_app_window) {
      bookmark_app_browser = b;
    } else {
      packaged_app_browser = b;
    }
  }
  ASSERT_TRUE(dev_tools_browser);
  ASSERT_TRUE(bookmark_app_browser);
  ASSERT_TRUE(bookmark_app_browser != browser());
  ASSERT_TRUE(packaged_app_browser);
  ASSERT_TRUE(packaged_app_browser != browser());
  ASSERT_TRUE(packaged_app_browser != bookmark_app_browser);

  EXPECT_FALSE(browser()->SupportsWindowFeature(Browser::FEATURE_WEBAPPFRAME));
  EXPECT_FALSE(
      dev_tools_browser->SupportsWindowFeature(Browser::FEATURE_WEBAPPFRAME));
  EXPECT_EQ(browser()->host_desktop_type() == chrome::HOST_DESKTOP_TYPE_ASH,
            bookmark_app_browser->SupportsWindowFeature(
                Browser::FEATURE_WEBAPPFRAME));
  EXPECT_FALSE(packaged_app_browser->SupportsWindowFeature(
      Browser::FEATURE_WEBAPPFRAME));

  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools_window);
}
