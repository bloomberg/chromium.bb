// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <string.h>

#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/filename_util.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class InProcessBrowserTestP
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<const char*> {
};

IN_PROC_BROWSER_TEST_P(InProcessBrowserTestP, TestP) {
  EXPECT_EQ(0, strcmp("foo", GetParam()));
}

INSTANTIATE_TEST_CASE_P(IPBTP,
                        InProcessBrowserTestP,
                        ::testing::Values("foo"));

// WebContents observer that can detect provisional load failures.
class LoadFailObserver : public content::WebContentsObserver {
 public:
  explicit LoadFailObserver(content::WebContents* contents)
      : content::WebContentsObserver(contents),
        failed_load_(false),
        error_code_(net::OK) { }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->GetNetErrorCode() == net::OK)
      return;

    failed_load_ = true;
    error_code_ = navigation_handle->GetNetErrorCode();
    validated_url_ = navigation_handle->GetURL();
  }

  bool failed_load() const { return failed_load_; }
  net::Error error_code() const { return error_code_; }
  const GURL& validated_url() const { return validated_url_; }

 private:
  bool failed_load_;
  net::Error error_code_;
  GURL validated_url_;

  DISALLOW_COPY_AND_ASSIGN(LoadFailObserver);
};

// Tests that InProcessBrowserTest cannot resolve external host, in this case
// "google.com" and "cnn.com". Using external resources is disabled by default
// in InProcessBrowserTest because it causes flakiness.
IN_PROC_BROWSER_TEST_F(InProcessBrowserTest, ExternalConnectionFail) {
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  const char* const kURLs[] = {
    "http://www.google.com/",
    "http://www.cnn.com/"
  };
  for (size_t i = 0; i < arraysize(kURLs); ++i) {
    GURL url(kURLs[i]);
    LoadFailObserver observer(contents);
    ui_test_utils::NavigateToURL(browser(), url);
    EXPECT_TRUE(observer.failed_load());
    EXPECT_EQ(net::ERR_NOT_IMPLEMENTED, observer.error_code());
    EXPECT_EQ(url, observer.validated_url());
  }
}

// Verify that AfterStartupTaskUtils considers startup to be complete
// prior to test execution so tasks posted by tests are never deferred.
IN_PROC_BROWSER_TEST_F(InProcessBrowserTest, AfterStartupTaskUtils) {
  EXPECT_TRUE(AfterStartupTaskUtils::IsBrowserStartupComplete());
}

// Paths are to very simple HTML files. One is accessible, the other is not.
const base::FilePath::CharType kPassHTML[] =
    FILE_PATH_LITERAL("chrome/test/data/accessibility_pass.html");
const base::FilePath::CharType kFailHTML[] =
    FILE_PATH_LITERAL("chrome/test/data/accessibility_fail.html");

/*
 * This class is meant as a test for the accessibility audit in the
 * InProcessBrowserTest. These tests do NOT validate the accessibility audit,
 * just the ability to run it.
 */
class InProcessAccessibilityBrowserTest : public InProcessBrowserTest {
 protected:
  // Construct a URL from a file path that can be used to get to a web page.
  base::FilePath BuildURLToFile(const base::FilePath::CharType* file_path) {
    base::FilePath source_root;
    if (!PathService::Get(base::DIR_SOURCE_ROOT, &source_root))
      return base::FilePath();
    return source_root.Append(file_path);
  }

  bool NavigateToURL(const base::FilePath::CharType* address) {
    GURL url = net::FilePathToFileURL(BuildURLToFile(address));

    if (!url.is_valid() || url.is_empty() || !browser())
      return false;

    ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
        browser(), url, 1);
    return true;
  }
};

// Test that an accessible page doesn't fail the accessibility audit.
IN_PROC_BROWSER_TEST_F(
    InProcessAccessibilityBrowserTest, DISABLED_VerifyAccessibilityPass) {
  ASSERT_TRUE(NavigateToURL(kPassHTML));

  std::string test_result;
  EXPECT_TRUE(RunAccessibilityChecks(&test_result));

  // No error message on success.
  EXPECT_EQ("", test_result);
}

// Test that a page that is not accessible will fail the accessibility audit.
IN_PROC_BROWSER_TEST_F(
    InProcessAccessibilityBrowserTest, VerifyAccessibilityFail) {
  ASSERT_TRUE(NavigateToURL(kFailHTML));

  std::string test_result;
  EXPECT_FALSE(RunAccessibilityChecks(&test_result));

  // Error should NOT be empty on failure.
  EXPECT_NE("", test_result);
}

}  // namespace
