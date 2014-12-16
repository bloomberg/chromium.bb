// Copyright 2013 The Chromium Authors. All rights reserved.
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

class FileManagerJsTest : public InProcessBrowserTest {
 public:
  // Runs all test functions in |file|, waiting for them to complete.
  void RunTest(const base::FilePath& file) {
    base::FilePath root_path;
    ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &root_path));

    const GURL url = net::FilePathToFileURL(
        root_path.Append(FILE_PATH_LITERAL("ui/file_manager/file_manager"))
            .Append(file));
    ui_test_utils::NavigateToURL(browser(), url);

    content::WebContents* const web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);

    const std::vector<int> empty_libraries;
    EXPECT_TRUE(ExecuteWebUIResourceTest(web_contents, empty_libraries));
  }
};

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, NavigationListModelTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("foreground/js/navigation_list_model_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileOperationHandlerTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("background/js/file_operation_handler_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ProgressCenterItemGroupTest) {
  RunTest(base::FilePath(FILE_PATH_LITERAL(
      "foreground/js/progress_center_item_group_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DeviceHandlerTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("background/js/device_handler_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MetadataCacheTest) {
  RunTest(base::FilePath(FILE_PATH_LITERAL(
      "foreground/js/metadata/metadata_cache_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, FileOperationManagerTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("background/js/file_operation_manager_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ImporterCommonTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("common/js/importer_common_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ImportHistoryTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("background/js/import_history_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, VolumeManagerTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("background/js/volume_manager_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, DirectoryTreeTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("foreground/js/ui/directory_tree_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MediaScannerTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("background/js/media_scanner_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, LRUCacheTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("common/js/lru_cache_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, MediaImportHandlerTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("background/js/media_import_handler_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, TaskQueueTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("background/js/task_queue_unittest.html")));
}
