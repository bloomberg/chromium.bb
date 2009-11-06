// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_layout_test.h"

// TODO(jorlow): Enable all of these tests, eventually...
static const char* kEventsFiles[] = {
  //"complex-values.html",
  //"iframe-events.html",
  //"index-get-and-set.html",
  //"onstorage-attribute-markup.html",
  //"onstorage-attribute-setattribute.html",
  //"onstorage-attribute-setwindow.html",
  "simple-events.html",
  //"string-conversion.html",
  //"window-open.html",
  NULL
};

static const char* kTopLevelFiles[] = {
  "clear.html",
  "quota.html",
  "remove-item.html",
  //"window-attributes-exist.html",
  NULL
};

static const char* kNoEventsFiles[] = {
  //"complex-keys.html",
  "delete-removal.html",
  "enumerate-storage.html",
  "enumerate-with-length-and-key.html",
  "simple-usage.html",
  NULL
};

static const char* kLocalStorageFiles[] = {
  //  "quota.html",
  NULL
};

static const char* kSessionStorageFiles[] = {
  //  "no-quota.html",
  NULL
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
    launch_arguments_.AppendSwitch(switches::kEnableSessionStorage);
    UILayoutTest::SetUp();
  }

  // We require fast/js/resources and storage/domstorage/script-tests for most
  // of the DOM Storage layout tests.  Add those to the list to be copied.
  void AddResources() {
    // Add other paths our tests require.
    FilePath js_dir = FilePath().AppendASCII("LayoutTests").
                      AppendASCII("fast").AppendASCII("js");
    AddResourceForLayoutTest(js_dir, FilePath().AppendASCII("resources"));
    AddResourceForLayoutTest(test_dir_, FilePath().AppendASCII("script-tests"));
  }

  // This is somewhat of a hack because we're running a real browser that
  // actually persists the LocalStorage state vs. DRT and TestShell which don't.
  // The correct fix is to fix the LayoutTests, but similar patches have been
  // rejected in the past.
  void ClearDOMStorage() {
    scoped_refptr<TabProxy> tab(GetActiveTab());
    ASSERT_TRUE(tab.get());

    GURL url = GetTestUrl(L"layout_tests", L"clear_dom_storage.html");
    tab->SetCookie(url, "");
    ASSERT_TRUE(tab->NavigateToURL(url));

    WaitUntilCookieNonEmpty(tab.get(), url, "cleared", kTestIntervalMs,
                            kTestWaitTimeoutMs);
  }

  // Runs each test in an array of strings until it hits a NULL.
  void RunTests(const char** files) {
    while (*files) {
      ClearDOMStorage();
      RunLayoutTest(*files, false);
      ++files;
    }
  }

  FilePath test_dir_;
};


TEST_F(DOMStorageTest, DOMStorageLayoutTests) {
  InitializeForLayoutTest(test_dir_, FilePath(), false);
  AddResources();
  RunTests(kTopLevelFiles);
}


TEST_F(DOMStorageTest, LocalStorageLayoutTests) {
  InitializeForLayoutTest(test_dir_, FilePath().AppendASCII("localstorage"),
                          false);
  AddResources();
  RunTests(kNoEventsFiles);
  RunTests(kEventsFiles);
  RunTests(kLocalStorageFiles);
}

TEST_F(DOMStorageTest, SessionStorageLayoutTests) {
  InitializeForLayoutTest(test_dir_, FilePath().AppendASCII("sessionstorage"),
                          false);
  AddResources();
  RunTests(kNoEventsFiles);
  //RunTests(kEventsFiles);
  RunTests(kSessionStorageFiles);
}
