// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_manager_jstest_base.h"

class FileManagerJsTest : public FileManagerJsTestBase {
 protected:
  FileManagerJsTest() : FileManagerJsTestBase(
      base::FilePath(FILE_PATH_LITERAL("ui/file_manager/file_manager"))) {}
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

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, ImportControllerTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("foreground/js/import_controller_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, AsyncUtilTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("common/js/async_util_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(FileManagerJsTest, TaskController) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("foreground/js/task_controller_unittest.html")));
}
