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
    FilePath js_dir = FilePath().
                      AppendASCII("fast").AppendASCII("js");
    AddResourceForLayoutTest(js_dir, FilePath().AppendASCII("resources"));
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
