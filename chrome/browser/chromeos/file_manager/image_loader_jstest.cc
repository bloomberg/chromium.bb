// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_manager_jstest_base.h"

class ImageLoaderJsTest : public FileManagerJsTestBase {
 protected:
  ImageLoaderJsTest() : FileManagerJsTestBase(
      base::FilePath(FILE_PATH_LITERAL("ui/file_manager/image_loader"))) {}
};

IN_PROC_BROWSER_TEST_F(ImageLoaderJsTest, ImageLoaderClientTest) {
  RunGeneratedTest("/image_loader_client_unittest.html");
}

IN_PROC_BROWSER_TEST_F(ImageLoaderJsTest, CacheTest) {
  RunGeneratedTest("/cache_unittest.html");
}

IN_PROC_BROWSER_TEST_F(ImageLoaderJsTest, ImageLoaderTest) {
  RunGeneratedTest("/image_loader_unittest.html");
}

IN_PROC_BROWSER_TEST_F(ImageLoaderJsTest, PiexLoaderTest) {
  RunGeneratedTest("/piex_loader_unittest.html");
}
