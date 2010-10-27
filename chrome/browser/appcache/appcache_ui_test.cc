// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "chrome/test/ui/ui_layout_test.h"

class AppCacheUITest : public UILayoutTest {
 protected:
  virtual ~AppCacheUITest() {}
};

// Flaky: http://crbug.com/54717
TEST_F(AppCacheUITest, FLAKY_AppCacheLayoutTests) {
  static const char* kLayoutTestFiles[] = {
      "404-manifest.html",
      "404-resource.html",
      "auth.html",
      "cyrillic-uri.html",
      "deferred-events-delete-while-raising.html",
      "deferred-events.html",
      "destroyed-frame.html",
      "detached-iframe.html",
      "different-origin-manifest.html",
      "different-scheme.html",
      "empty-manifest.html",
      "fallback.html",
      "foreign-iframe-main.html",
      "main-resource-hash.html",
      "manifest-containing-itself.html",
      "manifest-parsing.html",
      "manifest-redirect-2.html",
      "manifest-redirect.html",
      "manifest-with-empty-file.html",
      "navigating-away-while-cache-attempt-in-progress.html",
      "offline-access.html",
      "online-whitelist.html",
      "progress-counter.html",
      "reload.html",
      "remove-cache.html",
      "resource-redirect-2.html",
      "resource-redirect.html",
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

      // TODO(michaeln): investigate these more closely
      // "crash-when-navigating-away-then-back.html",
      // "credential-url.html",
      // "different-https-origin-resource-main.html",
      // "fail-on-update.html",
      // "idempotent-update.html", not sure this is a valid test
      // "local-content.html",
      // "max-size.html", we use a different quota scheme
      // "update-cache.html",  bug 38006
  };

  FilePath http_test_dir;
  http_test_dir = http_test_dir.AppendASCII("http");
  http_test_dir = http_test_dir.AppendASCII("tests");

  FilePath appcache_test_dir;
  appcache_test_dir = appcache_test_dir.AppendASCII("appcache");
  InitializeForLayoutTest(http_test_dir, appcache_test_dir, kHttpPort);

  StartHttpServer(new_http_root_dir_);
  for (size_t i = 0; i < arraysize(kLayoutTestFiles); ++i)
    RunLayoutTest(kLayoutTestFiles[i], kHttpPort);
  StopHttpServer();
}
