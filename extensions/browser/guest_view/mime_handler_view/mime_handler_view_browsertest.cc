// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

// The test extension id is set by the key value in the manifest.
const char* kExtensionId = "oickdpebdnfbgkcaoklfcdhjniefkcji";

class MimeHandlerViewTest : public ExtensionApiTest {
 public:
  ~MimeHandlerViewTest() override {}

  const extensions::Extension* LoadTestExtension() {
    const extensions::Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII("mime_handler_view"));
    if (!extension)
      return nullptr;

    CHECK_EQ(std::string(kExtensionId), extension->id());

    return extension;
  }

  void RunTest(const std::string& path) {
    const extensions::Extension* extension = LoadTestExtension();
    ASSERT_TRUE(extension);

    ASSERT_TRUE(StartEmbeddedTestServer());
    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir_.AppendASCII("mime_handler_view"));

    extensions::ResultCatcher catcher;

    ui_test_utils::NavigateToURL(browser(),
                                 embedded_test_server()->GetURL("/" + path));

    if (!catcher.GetNextResult())
      FAIL() << catcher.message();
  }
};

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, PostMessage) {
  RunTest("test_postmessage.html");
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, Basic) {
  RunTest("testBasic.csv");
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, Embedded) {
  RunTest("test_embedded.html");
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, Iframe) {
  RunTest("test_iframe.html");
}

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, Abort) {
  RunTest("testAbort.csv");
}
