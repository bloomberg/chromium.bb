// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "net/dns/mock_host_resolver.h"

const base::FilePath::CharType kFtpDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data");

class CrossOriginXHR : public ExtensionApiTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*.com", "127.0.0.1");
    ASSERT_TRUE(StartEmbeddedTestServer());
  }
};

IN_PROC_BROWSER_TEST_F(CrossOriginXHR, BackgroundPage) {
  ASSERT_TRUE(StartFTPServer(base::FilePath(kFtpDocRoot)));
  ASSERT_TRUE(RunExtensionTest("cross_origin_xhr/background_page")) << message_;
}

IN_PROC_BROWSER_TEST_F(CrossOriginXHR, AllURLs) {
  ASSERT_TRUE(RunExtensionTest("cross_origin_xhr/all_urls")) << message_;
}

IN_PROC_BROWSER_TEST_F(CrossOriginXHR, ContentScript) {
  ASSERT_TRUE(StartFTPServer(base::FilePath(kFtpDocRoot)));
  ASSERT_TRUE(RunExtensionTest("cross_origin_xhr/content_script")) << message_;
}

IN_PROC_BROWSER_TEST_F(CrossOriginXHR, FileAccess) {
  ASSERT_TRUE(RunExtensionTest("cross_origin_xhr/file_access")) << message_;
}

IN_PROC_BROWSER_TEST_F(CrossOriginXHR, NoFileAccess) {
  ASSERT_TRUE(RunExtensionTestNoFileAccess(
      "cross_origin_xhr/no_file_access")) << message_;
}
