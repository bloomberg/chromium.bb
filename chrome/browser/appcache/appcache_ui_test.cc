// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "chrome/test/ui/ui_layout_test.h"

class AppCacheUITest : public UILayoutTest {
 public:
  void RunAppCacheTests(const char* tests[], int num_tests) {
    FilePath http_test_dir;
    http_test_dir = http_test_dir.AppendASCII("http");
    http_test_dir = http_test_dir.AppendASCII("tests");

    FilePath appcache_test_dir;
    appcache_test_dir = appcache_test_dir.AppendASCII("appcache");
    InitializeForLayoutTest(http_test_dir, appcache_test_dir, kHttpPort);

    StartHttpServer(new_http_root_dir_);
    for (int i = 0; i < num_tests; ++i)
      RunLayoutTest(tests[i], kHttpPort);
    StopHttpServer();
  }

 protected:
  virtual ~AppCacheUITest() {}
};

// Flaky: http://crbug.com/54717
// The tests that don't depend on PHP should be less flaky.
TEST_F(AppCacheUITest, FLAKY_AppCacheLayoutTests_NoPHP) {
  static const char* kNoPHPTests[] = {
      "404-manifest.html",
      "404-resource.html",
      "cyrillic-uri.html",
      "deferred-events-delete-while-raising.html",
      "deferred-events.html",
      "destroyed-frame.html",
      "detached-iframe.html",
      "different-origin-manifest.html",
      "different-scheme.html",
      "empty-manifest.html",
      "foreign-iframe-main.html",
      "insert-html-element-with-manifest.html",
      "insert-html-element-with-manifest-2.html",
      "manifest-containing-itself.html",
      "manifest-parsing.html",
      "manifest-with-empty-file.html",
      "progress-counter.html",
      "reload.html",
      "simple.html",
      "top-frame-1.html",
      "top-frame-2.html",
      "top-frame-3.html",
      "top-frame-4.html",
      "whitelist-wildcard.html",
      "wrong-content-type.html",
      "wrong-signature-2.html",
      "wrong-signature.html",
      "xhr-foreign-resource.html",
  };

  // This test is racey.
  // https://bugs.webkit.org/show_bug.cgi?id=49104
  // "foreign-fallback.html"

  RunAppCacheTests(kNoPHPTests, arraysize(kNoPHPTests));
}

// Flaky: http://crbug.com/54717
// Lighty/PHP is not reliable enough on windows.
TEST_F(AppCacheUITest, FLAKY_AppCacheLayoutTests_PHP) {
  static const char* kPHPTests[] = {
      "auth.html",
      "fallback.html",
      "main-resource-hash.html",
      "manifest-redirect.html",
      "manifest-redirect-2.html",
      "navigating-away-while-cache-attempt-in-progress.html",
      "non-html.xhtml",
      "offline-access.html",
      "online-whitelist.html",
      "remove-cache.html",
      "resource-redirect.html",
      "resource-redirect-2.html",
      "update-cache.html",
  };

  // This tests loads a data url which calls notifyDone, this just
  // doesn't work with the layoutTestController in this test harness.
  // "fail-on-update.html",

  // Flaky for reasons i don't yet see?
  // "fail-on-update2.html",

  // TODO(michaeln): investigate these more closely
  // "crash-when-navigating-away-then-back.html",
  // "credential-url.html",
  // "different-https-origin-resource-main.html",
  // "idempotent-update.html", not sure this is a valid test
  // "local-content.html",
  // "max-size.html", we use a different quota scheme

  RunAppCacheTests(kPHPTests, arraysize(kPHPTests));
}
