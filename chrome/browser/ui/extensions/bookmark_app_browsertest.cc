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
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
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

typedef ExtensionBrowserTest BookmarkAppTest;

namespace {

// Used by ShouldLocationBarForXXX. Performs a navigation and then checks that
// the location bar visibility is as expcted.
void NavigateAndCheckForLocationBar(Browser* browser,
                                    const std::string& url_string,
                                    bool expected_visibility) {
  GURL url(url_string);
  ui_test_utils::NavigateToURL(browser, url);
  EXPECT_EQ(expected_visibility,
      browser->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR));
}

}  // namespace

// Check that the location bar is shown correctly for HTTP bookmark apps.
IN_PROC_BROWSER_TEST_F(BookmarkAppTest,
                       ShouldShowLocationBarForHTTPBookmarkApp) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableNewBookmarkApps);
  ASSERT_TRUE(test_server()->Start());

  // Load a http bookmark app.
  const Extension* http_bookmark_app = InstallExtensionWithSourceAndFlags(
      test_data_dir_.AppendASCII("app/"),
      1,
      extensions::Manifest::INTERNAL,
      extensions::Extension::FROM_BOOKMARK);
  ASSERT_TRUE(http_bookmark_app);

  // Launch it in a window.
  WebContents* app_window = OpenApplication(AppLaunchParams(
      browser()->profile(), http_bookmark_app,
      extensions::LAUNCH_CONTAINER_WINDOW, NEW_WINDOW,
      extensions::SOURCE_TEST));
  ASSERT_TRUE(app_window);

  // Find the new browser.
  Browser* http_app_browser = NULL;
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    std::string app_id =
        web_app::GetExtensionIdFromApplicationName((*it)->app_name());
    if (*it == browser()) {
      continue;
    } else if (app_id == http_bookmark_app->id()) {
      http_app_browser = *it;
    }
  }
  ASSERT_TRUE(http_app_browser);
  ASSERT_TRUE(http_app_browser != browser());

  // Navigate to the app's launch page; the location bar should be hidden.
  NavigateAndCheckForLocationBar(
      http_app_browser, "http://www.example.com/empty.html", false);

  // Navigate to another page on the same origin; the location bar should still
  // hidden.
  NavigateAndCheckForLocationBar(
      http_app_browser, "http://www.example.com/blah", false);

  // Navigate to the https version of the site; the location bar should
  // be hidden for both browsers.
  NavigateAndCheckForLocationBar(
      http_app_browser, "https://www.example.com/blah", false);

  // Navigate to different origin; the location bar should now be visible.
  NavigateAndCheckForLocationBar(
      http_app_browser, "http://www.foo.com/blah", true);

  // Navigate back to the app's origin; the location bar should now be hidden.
  NavigateAndCheckForLocationBar(
      http_app_browser, "http://www.example.com/blah", false);
}

// Check that the location bar is shown correctly for HTTPS bookmark apps.
IN_PROC_BROWSER_TEST_F(BookmarkAppTest,
                       ShouldShowLocationBarForHTTPSBookmarkApp) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableNewBookmarkApps);
  ASSERT_TRUE(test_server()->Start());

  // Load a https bookmark app.
  const Extension* https_bookmark_app = InstallExtensionWithSourceAndFlags(
      test_data_dir_.AppendASCII("https_app/"),
      1,
      extensions::Manifest::INTERNAL,
      extensions::Extension::FROM_BOOKMARK);
  ASSERT_TRUE(https_bookmark_app);

  // Launch it in a window.
  WebContents* app_window = OpenApplication(AppLaunchParams(
      browser()->profile(), https_bookmark_app,
      extensions::LAUNCH_CONTAINER_WINDOW, NEW_WINDOW,
      extensions::SOURCE_TEST));
  ASSERT_TRUE(app_window);

  // Find the new browser.
  Browser* https_app_browser = NULL;
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    std::string app_id =
        web_app::GetExtensionIdFromApplicationName((*it)->app_name());
    if (*it == browser()) {
      continue;
    } else if (app_id == https_bookmark_app->id()) {
      https_app_browser = *it;
    }
  }
  ASSERT_TRUE(https_app_browser);
  ASSERT_TRUE(https_app_browser != browser());

  // Navigate to the app's launch page; the location bar should be hidden.
  NavigateAndCheckForLocationBar(
      https_app_browser, "https://www.example.com/empty.html", false);

  // Navigate to another page on the same origin; the location bar should still
  // hidden.
  NavigateAndCheckForLocationBar(
      https_app_browser, "https://www.example.com/blah", false);

  // Navigate to the http version of the site; the location bar should
  // be visible for the https version as it is now on a less secure version
  // of its host.
  NavigateAndCheckForLocationBar(
      https_app_browser, "http://www.example.com/blah", true);

  // Navigate to different origin; the location bar should now be visible.
  NavigateAndCheckForLocationBar(
      https_app_browser, "http://www.foo.com/blah", true);

  // Navigate back to the app's origin; the location bar should now be hidden.
  NavigateAndCheckForLocationBar(
      https_app_browser, "https://www.example.com/blah", false);
}

// Open a normal browser window, a hosted app window, a legacy packaged app
// window and a dev tools window, and check that the web app frame feature is
// supported correctly.
IN_PROC_BROWSER_TEST_F(BookmarkAppTest, ShouldUseWebAppFrame) {
  ASSERT_TRUE(test_server()->Start());
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableWebAppFrame);

  // Load a hosted app.
   const Extension* bookmark_app = InstallExtensionWithSourceAndFlags(
      test_data_dir_.AppendASCII("app/"),
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
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("packaged_app/")));
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
  Browser* bookmark_app_browser = NULL;
  Browser* packaged_app_browser = NULL;
  Browser* dev_tools_browser = NULL;
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    if (*it == browser()) {
      continue;
    } else if ((*it)->app_name() == DevToolsWindow::kDevToolsApp) {
      dev_tools_browser = *it;
    } else if ((*it)->tab_strip_model()->GetActiveWebContents() ==
               bookmark_app_window) {
      bookmark_app_browser = *it;
    } else {
      packaged_app_browser = *it;
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
