// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/path_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/filename_util.h"

class GalleryJsTest : public InProcessBrowserTest {
 public:
  // Runs all test functions in |file|, waiting for them to complete.
  void RunTest(const base::FilePath& file) {
    base::FilePath root_path;
    ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &root_path));

    const GURL url = net::FilePathToFileURL(
        root_path.Append(FILE_PATH_LITERAL("ui/file_manager/gallery"))
            .Append(file));
    ui_test_utils::NavigateToURL(browser(), url);

    content::WebContents* const web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);

    const std::vector<int> empty_libraries;
    EXPECT_TRUE(ExecuteWebUIResourceTest(web_contents, empty_libraries));
  }
};

IN_PROC_BROWSER_TEST_F(GalleryJsTest, ExifEncoderTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("js/image_editor/exif_encoder_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(GalleryJsTest, ImageViewTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("js/image_editor/image_view_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(GalleryJsTest, EntryListWatcherTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("js/entry_list_watcher_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(GalleryJsTest, BackgroundTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("js/background_unittest.html")));
}
