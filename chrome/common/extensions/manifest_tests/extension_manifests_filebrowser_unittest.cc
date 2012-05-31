// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "base/string_number_conversions.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/file_browser_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

using extensions::Extension;

namespace errors = extension_manifest_errors;

TEST_F(ExtensionManifestTest, FileBrowserHandlers) {
  Testcase testcases[] = {
    Testcase("filebrowser_invalid_access_permission.json",
             ExtensionErrorUtils::FormatErrorMessage(
                 errors::kInvalidFileAccessValue, base::IntToString(1))),
    Testcase("filebrowser_invalid_access_permission_list.json",
             errors::kInvalidFileAccessList),
    Testcase("filebrowser_invalid_empty_access_permission_list.json",
             errors::kInvalidFileAccessList),
    Testcase("filebrowser_invalid_actions_1.json",
             errors::kInvalidFileBrowserHandler),
    Testcase("filebrowser_invalid_actions_2.json",
             errors::kInvalidFileBrowserHandler),
    Testcase("filebrowser_invalid_action_id.json",
             errors::kInvalidPageActionId),
    Testcase("filebrowser_invalid_action_title.json",
             errors::kInvalidPageActionDefaultTitle),
    Testcase("filebrowser_invalid_file_filters_1.json",
             errors::kInvalidFileFiltersList),
    Testcase("filebrowser_invalid_file_filters_2.json",
             ExtensionErrorUtils::FormatErrorMessage(
                errors::kInvalidFileFilterValue, base::IntToString(0))),
    Testcase("filebrowser_invalid_file_filters_url.json",
             ExtensionErrorUtils::FormatErrorMessage(
                errors::kInvalidURLPatternError, "http:*.html"))
  };
  RunTestcases(testcases, arraysize(testcases),
               EXPECT_TYPE_ERROR);

  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("filebrowser_valid.json"));
  ASSERT_TRUE(extension.get());
  ASSERT_TRUE(extension->file_browser_handlers() != NULL);
  ASSERT_EQ(extension->file_browser_handlers()->size(), 1U);
  const FileBrowserHandler* action =
      extension->file_browser_handlers()->at(0).get();
  EXPECT_EQ(action->title(), "Default title");
  EXPECT_EQ(action->icon_path(), "icon.png");
  const URLPatternSet& patterns = action->file_url_patterns();
  ASSERT_EQ(patterns.patterns().size(), 1U);
  ASSERT_TRUE(action->MatchesURL(
      GURL("filesystem:chrome-extension://foo/local/test.txt")));
  ASSERT_FALSE(action->HasCreateAccessPermission());
  ASSERT_TRUE(action->CanRead());
  ASSERT_TRUE(action->CanWrite());

  scoped_refptr<Extension> create_extension(
      LoadAndExpectSuccess("filebrowser_valid_with_create.json"));
  ASSERT_TRUE(create_extension->file_browser_handlers() != NULL);
  ASSERT_EQ(create_extension->file_browser_handlers()->size(), 1U);
  const FileBrowserHandler* create_action =
      create_extension->file_browser_handlers()->at(0).get();
  EXPECT_EQ(create_action->title(), "Default title");
  EXPECT_EQ(create_action->icon_path(), "icon.png");
  const URLPatternSet& create_patterns = create_action->file_url_patterns();
  ASSERT_EQ(create_patterns.patterns().size(), 0U);
  ASSERT_TRUE(create_action->HasCreateAccessPermission());
  ASSERT_FALSE(create_action->CanRead());
  ASSERT_FALSE(create_action->CanWrite());
}

TEST_F(ExtensionManifestTest, FileManagerURLOverride) {
  // A component extention can override chrome://files/ URL.
  std::string error;
  LoadExtension(Manifest("filebrowser_url_override.json"),
                &error, Extension::COMPONENT, Extension::NO_FLAGS);
#if defined(FILE_MANAGER_EXTENSION)
  EXPECT_EQ("", error);
#else
  EXPECT_EQ(std::string(errors::kInvalidChromeURLOverrides), error);
#endif

  // Extensions of other types can't ovverride chrome://files/ URL.
  LoadAndExpectError("filebrowser_url_override.json",
                     errors::kInvalidChromeURLOverrides);
}
