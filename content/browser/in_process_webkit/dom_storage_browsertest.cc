// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/common/content_switches.h"
#include "content/test/layout_browsertest.h"
#include "webkit/dom_storage/dom_storage_types.h"

#ifdef ENABLE_NEW_DOM_STORAGE_BACKEND
// TODO(michaeln) Ensure they pass.
#else
static const char* kRootFiles[] = {
  "clear.html",
//  "complex-keys.html",  // Output too big for a cookie.  crbug.com/33472
//  "complex-values.html",  // crbug.com/33472
  "quota.html",
  "remove-item.html",
  "window-attributes-exist.html",
  NULL
};

static const char* kEventsFiles[] = {
//  "basic-body-attribute.html",  // crbug.com/33472
//  "basic.html",  // crbug.com/33472
//  "basic-setattribute.html",  // crbug.com/33472
  "case-sensitive.html",
  "documentURI.html",
  NULL
};

static const char* kStorageFiles[] = {
  "delete-removal.html",
  "enumerate-storage.html",
  "enumerate-with-length-and-key.html",
  "index-get-and-set.html",
  "simple-usage.html",
  "string-conversion.html",
//  "window-open.html", // TODO(jorlow): Fix
  NULL
};

class DOMStorageTestBase : public InProcessBrowserLayoutTest {
 protected:
  DOMStorageTestBase(const FilePath& test_case_dir)
      : InProcessBrowserLayoutTest(FilePath(), test_case_dir) {
  }

  virtual ~DOMStorageTestBase() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kDisablePopupBlocking);
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    InProcessBrowserLayoutTest::SetUpInProcessBrowserTestFixture();
    AddResourceForLayoutTest(
        FilePath().AppendASCII("fast").AppendASCII("js"),
        FilePath().AppendASCII("resources"));
  }

  // This is somewhat of a hack because we're running a real browser that
  // actually persists the LocalStorage state vs. DRT and TestShell which don't.
  // The correct fix is to fix the LayoutTests, but similar patches have been
  // rejected in the past.
  void ClearDOMStorage() {
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScript(
        browser()->GetSelectedWebContents()->GetRenderViewHost(), L"",
        L"localStorage.clear();sessionStorage.clear();"));
  }

  // Runs each test in an array of strings until it hits a NULL.
  void RunTests(const char** files) {
    while (*files) {
      RunLayoutTest(*files);
      ClearDOMStorage();
      ++files;
    }
  }

  FilePath test_dir_;
};

class DOMStorageTest : public DOMStorageTestBase {
 public:
  DOMStorageTest()
      : DOMStorageTestBase(FilePath().AppendASCII("storage").
                                      AppendASCII("domstorage")) {
  }
  virtual ~DOMStorageTest() {}
};


// http://crbug.com/113611
IN_PROC_BROWSER_TEST_F(DOMStorageTest, FAILS_RootLayoutTests) {
  AddResourceForLayoutTest(FilePath(), FilePath().AppendASCII("script-tests"));
  RunTests(kRootFiles);
}

class DOMStorageTestEvents : public DOMStorageTestBase {
 public:
  DOMStorageTestEvents()
      : DOMStorageTestBase(FilePath().AppendASCII("storage").
                                      AppendASCII("domstorage").
                                      AppendASCII("events")) {
  }
  virtual ~DOMStorageTestEvents() {}
};

// Flakily fails on all platforms.  http://crbug.com/102641
IN_PROC_BROWSER_TEST_F(DOMStorageTestEvents, DISABLED_EventLayoutTests) {
  AddResourceForLayoutTest(
      FilePath(),
      FilePath().AppendASCII("storage").AppendASCII("domstorage").
          AppendASCII("script-tests"));
  RunTests(kEventsFiles);
}

class LocalStorageTest : public DOMStorageTestBase {
 public:
  LocalStorageTest()
      : DOMStorageTestBase(FilePath().AppendASCII("storage").
                                      AppendASCII("domstorage").
                                      AppendASCII("localstorage")) {
  }
  virtual ~LocalStorageTest() {}
};

#if defined(OS_LINUX)
// http://crbug.com/104872
#define MAYBE_LocalStorageLayoutTests FAILS_LocalStorageLayoutTests
#else
#define MAYBE_LocalStorageLayoutTests LocalStorageLayoutTests
#endif

IN_PROC_BROWSER_TEST_F(LocalStorageTest, MAYBE_LocalStorageLayoutTests) {
  RunTests(kStorageFiles);
}

class SessionStorageTest : public DOMStorageTestBase {
 public:
  SessionStorageTest()
      : DOMStorageTestBase(FilePath().AppendASCII("storage").
                                      AppendASCII("domstorage").
                                      AppendASCII("sessionstorage")) {
  }
  virtual ~SessionStorageTest() {}
};

#if defined(OS_LINUX)
// http://crbug.com/104872
#define MAYBE_SessionStorageLayoutTests FAILS_SessionStorageLayoutTests
#else
#define MAYBE_SessionStorageLayoutTests SessionStorageLayoutTests
#endif

IN_PROC_BROWSER_TEST_F(SessionStorageTest, MAYBE_SessionStorageLayoutTests) {
  RunTests(kStorageFiles);
}

#endif  // ENABLE_NEW_DOM_STORAGE_BACKEND
