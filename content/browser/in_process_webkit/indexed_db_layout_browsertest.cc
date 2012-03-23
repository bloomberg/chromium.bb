// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "content/test/layout_browsertest.h"

class IndexedDBLayoutTest : public InProcessBrowserLayoutTest {
 public:
  IndexedDBLayoutTest() : InProcessBrowserLayoutTest(
      FilePath().AppendASCII("storage").AppendASCII("indexeddb")) {
  }
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    InProcessBrowserLayoutTest::SetUpInProcessBrowserTestFixture();
    AddResourceForLayoutTest(
        FilePath().AppendASCII("fast").AppendASCII("js"),
        FilePath().AppendASCII("resources"));
  }
};

namespace {

static const char* kLayoutTestFileNames[] = {
  "basics.html",
  "basics-shared-workers.html",
  "basics-workers.html",
  "index-basics.html",
  "objectstore-basics.html",
  "prefetch-bugfix-108071.html",
};

}

IN_PROC_BROWSER_TEST_F(IndexedDBLayoutTest, FirstTest) {
  for (size_t i = 0; i < arraysize(kLayoutTestFileNames); ++i)
    RunLayoutTest(kLayoutTestFileNames[i]);
}
