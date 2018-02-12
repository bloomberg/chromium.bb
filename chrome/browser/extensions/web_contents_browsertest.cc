// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_api_frame_id_map.h"

namespace {

content::WebContents* GetActiveWebContents(const Browser* browser) {
  return browser->tab_strip_model()->GetActiveWebContents();
}

}  // namespace

// Tests that we can load extension pages into the tab area and they can call
// extension APIs.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, WebContents) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                    .AppendASCII("1.0.0.0")));

  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/page.html"));

  bool result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetActiveWebContents(browser()), "testTabsAPI()", &result));
  EXPECT_TRUE(result);

  // There was a bug where we would crash if we navigated to a page in the same
  // extension because no new render view was getting created, so we would not
  // do some setup.
  ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/page.html"));
  result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      GetActiveWebContents(browser()), "testTabsAPI()", &result));
  EXPECT_TRUE(result);
}

// Test that we cache frame data for all frames on creation. Regression test for
// crbug.com/810614.
IN_PROC_BROWSER_TEST_F(ExtensionBrowserTest, FrameDataCached) {
  // Load an extension with a web accessible resource.
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("web_accessible_resources"));
  ASSERT_TRUE(extension);

  ASSERT_TRUE(embedded_test_server()->Start());

  // Some utility functions for the test.

  // Returns whether the frame data for |rfh| is cached.
  auto has_cached_frame_data = [](content::RenderFrameHost* rfh) {
    return extensions::ExtensionApiFrameIdMap::Get()
        ->HasCachedFrameDataForTesting(rfh);
  };

  // Adds an iframe with the given |name| and |src| to the given |web_contents|
  // and waits till it loads. Returns true if successful.
  auto add_iframe = [](content::WebContents* web_contents,
                       const std::string& name, const GURL& src) {
    content::TestNavigationObserver observer(web_contents,
                                             1 /*number_of_navigations*/);
    const char* code = R"(
      var iframe = document.createElement('iframe');
      iframe.name = '%s';
      iframe.src = '%s';
      document.body.appendChild(iframe);
    )";
    content::ExecuteScriptAsync(
        web_contents,
        base::StringPrintf(code, name.c_str(), src.spec().c_str()));

    observer.WaitForNavigationFinished();
    return observer.last_navigation_succeeded() &&
           observer.last_navigation_url() == src;
  };

  // Returns the frame with the given |name| in |web_contents|.
  auto get_frame_by_name = [](content::WebContents* web_contents,
                              const std::string& name) {
    return content::FrameMatchingPredicate(
        web_contents, base::Bind(&content::FrameMatchesName, name));
  };

  // Navigates the browser to |url|. Injects a web-frame and an extension frame
  // into the page and ensures that extension frame data is cached for each
  // created frame.
  auto load_page_and_test = [&](const GURL& url) {
    SCOPED_TRACE(base::StringPrintf("Testing %s", url.spec().c_str()));

    ui_test_utils::NavigateToURL(browser(), url);
    content::WebContents* web_contents = GetActiveWebContents(browser());

    // Ensure that the frame data for the main frame is cached.
    EXPECT_TRUE(has_cached_frame_data(web_contents->GetMainFrame()));

    // Add an extension iframe to the page and ensure its frame data is cached.
    ASSERT_TRUE(
        add_iframe(web_contents, "extension_frame",
                   extension->GetResourceURL("web_accessible_page.html")));
    EXPECT_TRUE(has_cached_frame_data(
        get_frame_by_name(web_contents, "extension_frame")));

    // Add a web frame to the page and ensure its frame data is cached.
    ASSERT_TRUE(add_iframe(web_contents, "web_frame",
                           embedded_test_server()->GetURL("/empty.html")));
    EXPECT_TRUE(
        has_cached_frame_data(get_frame_by_name(web_contents, "web_frame")));
  };
  // End utility functions.

  // Test an extension page.
  load_page_and_test(extension->GetResourceURL("extension_page.html"));

  // Test a non-extension page.
  load_page_and_test(embedded_test_server()->GetURL("/empty.html"));
}
