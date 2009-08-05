// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Completely disable for now.
#if 0

#include "chrome/common/chrome_switches.h"
#include "chrome/test/ui/ui_layout_test.h"

// TODO(jorlow): Enable these tests when we remove them from the
//               test_exceptions.txt file.
//static const char* kTopLevelFiles[] = {
  //"window-attributes-exist.html"
//};

// TODO(jorlow): Enable these tests when we remove them from the
//               test_exceptions.txt file.
static const char* kSubDirFiles[] = {
  "clear.html",
  "delete-removal.html",
  "enumerate-storage.html",
  "enumerate-with-length-and-key.html",
  //"iframe-events.html",
  //"index-get-and-set.html",
  //"onstorage-attribute-markup.html",
  //"onstorage-attribute-setattribute.html",
  //"localstorage/onstorage-attribute-setwindow.html",
  //"simple-events.html",
  "simple-usage.html",
  //"string-conversion.html",
//  "window-open.html"
};

class DOMStorageTest : public UILayoutTest {
 protected:
  DOMStorageTest()
      : UILayoutTest(),
        test_dir_(FilePath().AppendASCII("LayoutTests").
                  AppendASCII("storage").AppendASCII("domstorage"))
  {
  }

  virtual ~DOMStorageTest() { }

  virtual void SetUp() {
    launch_arguments_.AppendSwitch(switches::kDisablePopupBlocking);
    launch_arguments_.AppendSwitch(switches::kEnableLocalStorage);
    launch_arguments_.AppendSwitch(switches::kEnableSessionStorage);
    UILayoutTest::SetUp();
  }

  FilePath test_dir_;
};

TEST_F(DOMStorageTest, DISABLED_DOMStorageLayoutTests) {
  // TODO(jorlow): Enable these tests when we remove them from the
  //               test_exceptions.txt file.
  //InitializeForLayoutTest(test_dir_, FilePath(), false);
  //for (size_t i=0; i<arraysize(kTopLevelFiles); ++i)
  //  RunLayoutTest(kTopLevelFiles[i], false, true);
}

TEST_F(DOMStorageTest, DISABLED_LocalStorageLayoutTests) {
  InitializeForLayoutTest(test_dir_, FilePath().AppendASCII("localstorage"),
                          false);
  for (size_t i=0; i<arraysize(kSubDirFiles); ++i)
    RunLayoutTest(kSubDirFiles[i], false);
}

TEST_F(DOMStorageTest, DISABLED_SessionStorageLayoutTests) {
  InitializeForLayoutTest(test_dir_, FilePath().AppendASCII("sessionstorage"),
                          false);
  for (size_t i=0; i<arraysize(kSubDirFiles); ++i)
    RunLayoutTest(kSubDirFiles[i], false);
}

#endif

