// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/layout_browsertest.h"

class BlobLayoutTest : public InProcessBrowserLayoutTest {
 public:
  BlobLayoutTest() :
      InProcessBrowserLayoutTest(
          FilePath(),
          FilePath(FILE_PATH_LITERAL("fast/files")).NormalizePathSeparators()) {
  }
};

IN_PROC_BROWSER_TEST_F(BlobLayoutTest, SliceTests) {
  RunLayoutTest("blob-slice-test.html");
}
