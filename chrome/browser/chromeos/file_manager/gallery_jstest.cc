// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_manager_jstest_base.h"

class GalleryJsTest : public FileManagerJsTestBase {
 protected:
  GalleryJsTest() : FileManagerJsTestBase(
      base::FilePath(FILE_PATH_LITERAL("ui/file_manager/gallery/js"))) {}
};

IN_PROC_BROWSER_TEST_F(GalleryJsTest, ExifEncoderTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("image_editor/exif_encoder_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(GalleryJsTest, ImageViewTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("image_editor/image_view_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(GalleryJsTest, EntryListWatcherTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("entry_list_watcher_unittest.html")));
}

IN_PROC_BROWSER_TEST_F(GalleryJsTest, BackgroundTest) {
  RunTest(base::FilePath(
      FILE_PATH_LITERAL("background_unittest.html")));
}
