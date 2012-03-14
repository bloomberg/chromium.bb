// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/test/test_timeouts.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/ui/ui_layout_test.h"
#include "content/public/common/content_switches.h"
#include "net/base/net_util.h"

static const char* kLayoutTestFileNames[] = {
  // TODO(dgrogan): Put the other IDB layout tests here.
  "prefetch-bugfix-108071.html",
  "basics.html",
//  "objectstore-basics.html", // Too big: crbug.com/33472
//  "index-basics.html", // Too big.
};

static const char* kWorkerTestFileNames[] = {
  "basics-workers.html",
  "basics-shared-workers.html",
//  "objectstore-basics-workers.html",  // Too big.
};

class IndexedDBUILayoutTest : public UILayoutTest {
 protected:
  IndexedDBUILayoutTest()
      : UILayoutTest(),
        test_dir_(FilePath().
                  AppendASCII("storage").AppendASCII("indexeddb")) {
  }

  virtual ~IndexedDBUILayoutTest() { }

  void AddJSTestResources() {
    // Add other paths our tests require.
    AddResourceForLayoutTest(
        FilePath().AppendASCII("fast").AppendASCII("js"),
        FilePath().AppendASCII("resources"));
    AddResourceForLayoutTest(
        FilePath().AppendASCII("fast").AppendASCII("filesystem"),
        FilePath().AppendASCII("resources"));
  }

  FilePath test_dir_;
};

TEST_F(IndexedDBUILayoutTest, LayoutTests) {
  const int port = kNoHttpPort;
  InitializeForLayoutTest(test_dir_, FilePath(), port);
  AddJSTestResources();
  for (size_t i = 0; i < arraysize(kLayoutTestFileNames); ++i)
    RunLayoutTest(kLayoutTestFileNames[i], port);
}

TEST_F(IndexedDBUILayoutTest, WorkerLayoutTests) {
  const int port = kNoHttpPort;
  InitializeForLayoutTest(test_dir_, FilePath(), port);
  AddJSTestResources();
  for (size_t i = 0; i < arraysize(kWorkerTestFileNames); ++i)
    RunLayoutTest(kWorkerTestFileNames[i], port);
}
