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

// The test extension id is set by the key value in the manifest.
const char* kExtensionId = "oickdpebdnfbgkcaoklfcdhjniefkcji";

class MimeHandlerViewTest : public ExtensionApiTest {
 public:
  ~MimeHandlerViewTest() override {}

  const extensions::Extension* LoadTestExtension() {
    const extensions::Extension* extension = LoadExtension(
        test_data_dir_.AppendASCII("mime_handler_view"));
    if (!extension)
      return nullptr;

    CHECK_EQ(std::string(kExtensionId), extension->id());

    return extension;
  }
};

// Not working on Windows because of crbug.com/443466.
#if defined(OS_WIN)
#define MAYBE_PostMessage DISABLED_PostMessage
#else
#define MAYBE_PostMessage PostMessage
#endif

IN_PROC_BROWSER_TEST_F(MimeHandlerViewTest, MAYBE_PostMessage) {
  const extensions::Extension* extension = LoadTestExtension();
  ASSERT_TRUE(extension);

  extensions::ResultCatcher catcher;

  GURL extension_url("chrome-extension://" +
                     std::string(kExtensionId) +
                     "/test_postmessage.html");
  ui_test_utils::NavigateToURL(browser(), extension_url);

  if (!catcher.GetNextResult())
    FAIL() << catcher.message();
}
