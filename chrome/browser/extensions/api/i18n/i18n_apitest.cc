// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/ui_test_utils.h"
#include "net/test/test_server.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, I18N) {
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionTest("i18n")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, I18NUpdate) {
  ASSERT_TRUE(StartTestServer());
  // Create an Extension whose messages.json file will be updated.
  base::ScopedTempDir extension_dir;
  ASSERT_TRUE(extension_dir.CreateUniqueTempDir());
  file_util::CopyFile(
      test_data_dir_.AppendASCII("i18nUpdate")
                    .AppendASCII("manifest.json"),
      extension_dir.path().AppendASCII("manifest.json"));
  file_util::CopyFile(
      test_data_dir_.AppendASCII("i18nUpdate")
                    .AppendASCII("contentscript.js"),
      extension_dir.path().AppendASCII("contentscript.js"));
  file_util::CopyDirectory(
      test_data_dir_.AppendASCII("i18nUpdate")
                    .AppendASCII("_locales"),
      extension_dir.path().AppendASCII("_locales"),
      true);

  const extensions::Extension* extension = LoadExtension(extension_dir.path());

  ResultCatcher catcher;

  // Test that the messages.json file is loaded and the i18n message is loaded.
  ui_test_utils::NavigateToURL(
      browser(), test_server()->GetURL("file/extensions/test_file.html"));
  EXPECT_TRUE(catcher.GetNextResult());

  string16 title;
  ui_test_utils::GetCurrentTabTitle(browser(), &title);
  EXPECT_EQ(std::string("FIRSTMESSAGE"), UTF16ToUTF8(title));

  // Change messages.json file and reload extension.
  file_util::CopyFile(
      test_data_dir_.AppendASCII("i18nUpdate")
                    .AppendASCII("messages2.json"),
      extension_dir.path().AppendASCII("_locales/en/messages.json"));
  ReloadExtension(extension->id());

  // Check that the i18n message is also changed.
  ui_test_utils::NavigateToURL(
      browser(), test_server()->GetURL("file/extensions/test_file.html"));
  EXPECT_TRUE(catcher.GetNextResult());

  ui_test_utils::GetCurrentTabTitle(browser(), &title);
  EXPECT_EQ(std::string("SECONDMESSAGE"), UTF16ToUTF8(title));
}
