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

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    InProcessBrowserLayoutTest::SetUpInProcessBrowserTestFixture();
    AddResourceForLayoutTest(
        FilePath(FILE_PATH_LITERAL("fast/js")).NormalizePathSeparators(),
        FilePath(FILE_PATH_LITERAL("resources")));
  }

  void RunLayoutTests(const char* file_names[]) {
    for (size_t i = 0; file_names[i]; i++)
      RunLayoutTest(file_names[i]);
  }
};

namespace {

static const char* kSliceTests[] = {
  "blob-slice-test.html",
  NULL
};

}

IN_PROC_BROWSER_TEST_F(BlobLayoutTest, SliceTests) {
  RunLayoutTests(kSliceTests);
}
