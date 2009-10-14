// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/ui/ui_layout_test.h"

// TODO(jorlow): Enable these tests when we remove them from the
//               test_exceptions.txt file.
// static const char* kTopLevelFiles[] = {
//   "window-attributes-exist.html"
// };

// TODO(jorlow): Enable these tests when we remove them from the
//               test_exceptions.txt file.
static const char* kSubDirFiles[] = {
  //"clear.html",
  "delete-removal.html",
  "enumerate-storage.html",
  "enumerate-with-length-and-key.html",
  // "iframe-events.html",
  // "index-get-and-set.html",
  // "onstorage-attribute-markup.html",
  // "onstorage-attribute-setattribute.html",
  // "localstorage/onstorage-attribute-setwindow.html",
  // "simple-events.html",
  "simple-usage.html",
  // "string-conversion.html",
  // "window-open.html"
};

class DOMStorageTest : public UILayoutTest {
 protected:
  DOMStorageTest()
      : UILayoutTest(),
        test_dir_(FilePath().AppendASCII("LayoutTests").
                  AppendASCII("storage").AppendASCII("domstorage")) {
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

TEST_F(DOMStorageTest, DOMStorageLayoutTests) {
  // TODO(jorlow): Enable these tests when we remove them from the
  //               test_exceptions.txt file.
  //InitializeForLayoutTest(test_dir_, FilePath(), false);
  //for (size_t i=0; i<arraysize(kTopLevelFiles); ++i)
  //  RunLayoutTest(kTopLevelFiles[i], false, true);
}

// http://code.google.com/p/chromium/issues/detail?id=24145
// Remove build_config.h include when this is removed.
#if defined(OS_WIN)
#define MAYBE_LocalStorageLayoutTests FLAKY_LocalStorageLayoutTests
#else
#define MAYBE_LocalStorageLayoutTests LocalStorageLayoutTests
#endif  // defined(OS_WIN)

TEST_F(DOMStorageTest, MAYBE_LocalStorageLayoutTests) {
  InitializeForLayoutTest(test_dir_, FilePath().AppendASCII("localstorage"),
                          false);
  for (size_t i = 0; i < arraysize(kSubDirFiles); ++i)
    RunLayoutTest(kSubDirFiles[i], false);
}

TEST_F(DOMStorageTest, SessionStorageLayoutTests) {
  InitializeForLayoutTest(test_dir_, FilePath().AppendASCII("sessionstorage"),
                          false);
  for (size_t i = 0; i < arraysize(kSubDirFiles); ++i)
    RunLayoutTest(kSubDirFiles[i], false);
}
