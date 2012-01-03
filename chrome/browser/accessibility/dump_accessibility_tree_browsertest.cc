// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/logging.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

using content::OpenURLParams;
using content::Referrer;

// Suffix of the expectation file corresponding to html file.
// Example:
// HTML test:      test-file.html
// Expected:       test-file-expected-mac.txt.
// Auto-generated: test-file-actual-mac.txt
#if defined(OS_WIN)
static const std::string kActualFileSuffix = "-actual-win.txt";
static const std::string kExpectedFileSuffix  = "-expected-win.txt";
#elif defined(OS_MACOSX)
static const std::string kActualFileSuffix = "-actual-mac.txt";
static const std::string kExpectedFileSuffix = "-expected-mac.txt";
#else
#error DumpAccessibilityTree does not support this platform.
#endif

// HTML id attribute prefix identifying a node to test.
static const std::string kTestId = "test";

// Required to enter html content into a url.
static const std::string kUrlPreamble = "data:text/html,\n<!doctype html>";

// Dumps a BrowserAccessibility tree into a string.
void DumpAccessibilityTree(BrowserAccessibility* node,
                           std::string* contents) {
  *contents += node->ToString() + "\n";
  for (size_t i = 0; i < node->children().size(); ++i)
    DumpAccessibilityTree(node->children()[i], contents);
}

// This test takes a snapshot of the platform BrowserAccessibility tree and
// tests it against an expected baseline.
//
// The flow of the test is as outlined below.
// 1. Load an html file from chrome/test/data/accessibility.
// 2. Read the expectation.
// 3. Browse to the page and serialize the platform specific tree into a human
//    readable string.
// 4. Perform a comparison between actual and expected and fail if they do not
//    exactly match.
class DumpAccessibilityTreeTest : public InProcessBrowserTest {
 public:
  virtual void SetUpInProcessBrowserTestFixture() {
    FilePath resources_pack_path;
    EXPECT_TRUE(PathService::Get(chrome::FILE_RESOURCES_PACK,
                                 &resources_pack_path));
    ResourceBundle::AddDataPackToSharedInstance(resources_pack_path);
  }
};

IN_PROC_BROWSER_TEST_F(DumpAccessibilityTreeTest,
                       PlatformTreeDifferenceTest) {
  RenderWidgetHostView* host_view =
      browser()->GetSelectedWebContents()->GetRenderWidgetHostView();
  RenderWidgetHost* host = host_view->GetRenderWidgetHost();
  RenderViewHost* view_host = static_cast<RenderViewHost*>(host);
  view_host->set_save_accessibility_tree_for_testing(true);
  view_host->EnableRendererAccessibility();

  // Setup test paths.
  FilePath dir_test_data;
  EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &dir_test_data));
  FilePath test_path(dir_test_data.Append(FilePath("accessibility")));
  EXPECT_TRUE(file_util::PathExists(test_path))
      << test_path.LossyDisplayName();

  // Grab all HTML files.
  file_util::FileEnumerator file_enumerator(test_path,
                                            false,
                                            file_util::FileEnumerator::FILES,
                                            "*.html");

  // TODO(dtseng): Make each of these a gtest with script.
  FilePath html_file;
  while (!(html_file = file_enumerator.Next()).empty()) {
    std::string html_contents;
    file_util::ReadFileToString(html_file, &html_contents);

    std::string expected_contents;
    FilePath expected_file =
        FilePath(html_file.RemoveExtension().value() + kExpectedFileSuffix);
    file_util::ReadFileToString(
        expected_file,
        &expected_contents);

    // Load the page.
    ui_test_utils::WindowedNotificationObserver tree_updated_observer(
        content::NOTIFICATION_RENDER_VIEW_HOST_ACCESSIBILITY_TREE_UPDATED,
        content::NotificationService::AllSources());
    GURL url(kUrlPreamble + html_contents);
    browser()->OpenURL(OpenURLParams(
        url, Referrer(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED, false));

    // Wait for the tree.
    tree_updated_observer.Wait();

    // Perform a diff (or write the initial baseline).
    std::string actual_contents;
    DumpAccessibilityTree(
        host_view->GetBrowserAccessibilityManager()->GetRoot(),
        &actual_contents);
    EXPECT_EQ(expected_contents, actual_contents);

    if (!file_util::PathExists(expected_file)) {
      FilePath actual_file =
          FilePath(html_file.RemoveExtension().value() + kActualFileSuffix);
      EXPECT_TRUE(file_util::WriteFile(
          actual_file, actual_contents.c_str(), actual_contents.size()));

      ADD_FAILURE() << "No expectation found. Create it by doing:\n"
          << "mv " << actual_file.LossyDisplayName() << " "
          << expected_file.LossyDisplayName();
    }
  }
}
