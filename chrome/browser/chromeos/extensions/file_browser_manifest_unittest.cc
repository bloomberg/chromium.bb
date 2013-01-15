// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_number_conversions.h"
#include "chrome/browser/chromeos/extensions/file_browser_handler.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/extensions/value_builder.h"
#include "extensions/common/error_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;

using extensions::DictionaryBuilder;
using extensions::Extension;
using extensions::ExtensionBuilder;
using extensions::ListBuilder;

namespace {

class FileBrowserHandlerManifestTest : public ExtensionManifestTest {
  virtual void SetUp() OVERRIDE {
    ExtensionManifestTest::SetUp();
    extensions::ManifestHandler::Register(
        extension_manifest_keys::kFileBrowserHandlers,
        new FileBrowserHandlerParser);
    extensions::ManifestHandler::Register(
        keys::kChromeURLOverrides,
        new extensions::URLOverridesHandler);
  }
};

TEST_F(FileBrowserHandlerManifestTest, InvalidFileBrowserHandlers) {
  Testcase testcases[] = {
    Testcase("filebrowser_invalid_access_permission.json",
             extensions::ErrorUtils::FormatErrorMessage(
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
             extensions::ErrorUtils::FormatErrorMessage(
                errors::kInvalidFileFilterValue, base::IntToString(0))),
    Testcase("filebrowser_invalid_file_filters_url.json",
             extensions::ErrorUtils::FormatErrorMessage(
                errors::kInvalidURLPatternError, "http:*.html"))
  };
  RunTestcases(testcases, arraysize(testcases), EXPECT_TYPE_ERROR);
}

TEST_F(FileBrowserHandlerManifestTest, ValidFileBrowserHandler) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
      .SetManifest(DictionaryBuilder()
                   .Set("name", "file browser handler test")
                   .Set("version", "1.0.0")
                   .Set("manifest_version", 2)
                   .Set("file_browser_handlers", ListBuilder()
                       .Append(DictionaryBuilder()
                           .Set("id", "ExtremelyCoolAction")
                           .Set("default_title", "Be Amazed")
                           .Set("default_icon", "icon.png")
                           .Set("file_filters", ListBuilder()
                               .Append("filesystem:*.txt")))))
      .Build();

  ASSERT_TRUE(extension.get());
  FileBrowserHandler::List* handlers =
      FileBrowserHandler::GetHandlers(extension);
  ASSERT_TRUE(handlers != NULL);
  ASSERT_EQ(handlers->size(), 1U);
  const FileBrowserHandler* action = handlers->at(0).get();

  EXPECT_EQ(action->id(), "ExtremelyCoolAction");
  EXPECT_EQ(action->title(), "Be Amazed");
  EXPECT_EQ(action->icon_path(), "icon.png");
  const extensions::URLPatternSet& patterns = action->file_url_patterns();
  ASSERT_EQ(patterns.patterns().size(), 1U);
  EXPECT_TRUE(action->MatchesURL(
      GURL("filesystem:chrome-extension://foo/local/test.txt")));
  EXPECT_FALSE(action->HasCreateAccessPermission());
  EXPECT_TRUE(action->CanRead());
  EXPECT_TRUE(action->CanWrite());
  EXPECT_FALSE(action->CanHandleMIMEType("plain/text"));
}

TEST_F(FileBrowserHandlerManifestTest, ValidFileBrowserHandlerMIMETypes) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
      .SetID(extension_misc::kQuickOfficeExtensionId)
      .SetManifest(DictionaryBuilder()
                   .Set("name", "file browser handler test")
                   .Set("version", "1.0.0")
                   .Set("manifest_version", 2)
                   .Set("file_browser_handlers", ListBuilder()
                       .Append(DictionaryBuilder()
                           .Set("id", "ID")
                           .Set("default_title", "Default title")
                           .Set("default_icon", "icon.png")
                           .Set("file_filters", ListBuilder()
                               .Append("filesystem:*.txt"))
                           .Set("mime_types", ListBuilder()
                               .Append("plain/text")))))
      .Build();

  ASSERT_TRUE(extension.get());
  FileBrowserHandler::List* handlers =
      FileBrowserHandler::GetHandlers(extension);
  ASSERT_TRUE(handlers != NULL);
  ASSERT_EQ(handlers->size(), 1U);
  const FileBrowserHandler* action = handlers->at(0).get();

  EXPECT_FALSE(action->CanHandleMIMEType("plain/html"));
  EXPECT_TRUE(action->CanHandleMIMEType("plain/text"));

  const extensions::URLPatternSet& patterns = action->file_url_patterns();
  ASSERT_EQ(patterns.patterns().size(), 1U);
  EXPECT_TRUE(action->MatchesURL(
      GURL("filesystem:chrome-extension://foo/local/test.txt")));
}

TEST_F(FileBrowserHandlerManifestTest,
       FileBrowserHandlerMIMETypesNotWhitelisted) {
  scoped_ptr<DictionaryValue> manifest_value =
      DictionaryBuilder()
          .Set("name", "MIME types test")
          .Set("version", "1.0.0")
          .Set("manifest_version", 2)
          .Set("file_browser_handlers", ListBuilder()
              .Append(DictionaryBuilder()
                  .Set("id", "ID")
                  .Set("default_title", "Default title")
                  .Set("default_icon", "icon.png")
                  .Set("file_filters", ListBuilder()
                      .Append("filesystem:*.txt"))
                  .Set("mime_types", ListBuilder()
                      .Append("plain/text"))))
      .Build();

  LoadAndExpectError(Manifest(manifest_value.get(), "MIME types test"),
                     errors::kNoPermissionForFileBrowserHandlerMIMETypes);
}

TEST_F(FileBrowserHandlerManifestTest, ValidFileBrowserHandlerWithCreate) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
      .SetManifest(DictionaryBuilder()
                   .Set("name", "file browser handler test create")
                   .Set("version", "1.0.0")
                   .Set("manifest_version", 2)
                   .Set("file_browser_handlers", ListBuilder()
                       .Append(DictionaryBuilder()
                           .Set("id", "ID")
                           .Set("default_title", "Default title")
                           .Set("default_icon", "icon.png")
                           .Set("file_filters", ListBuilder()
                               .Append("filesystem:*.txt"))
                           .Set("file_access", ListBuilder()
                               .Append("create")))))
      .Build();

  ASSERT_TRUE(extension.get());
  FileBrowserHandler::List* handlers =
      FileBrowserHandler::GetHandlers(extension);
  ASSERT_TRUE(handlers != NULL);
  ASSERT_EQ(handlers->size(), 1U);
  const FileBrowserHandler* action = handlers->at(0).get();
  const extensions::URLPatternSet& patterns = action->file_url_patterns();

  EXPECT_EQ(patterns.patterns().size(), 0U);
  EXPECT_TRUE(action->HasCreateAccessPermission());
  EXPECT_FALSE(action->CanRead());
  EXPECT_FALSE(action->CanWrite());
}

TEST_F(FileBrowserHandlerManifestTest, FileManagerURLOverride) {
  scoped_ptr<DictionaryValue> manifest_value =
      DictionaryBuilder()
          .Set("name", "override_files")
          .Set("version", "1.0.0")
          .Set("manifest_version", 2)
          .Set("chrome_url_overrides", DictionaryBuilder()
              .Set("files", "main.html"))
      .Build();

  // Non component extensions can't ovverride chrome://files/ URL.
  LoadAndExpectError(Manifest(manifest_value.get(), "override_files"),
                     errors::kInvalidChromeURLOverrides);

  // A component extention can override chrome://files/ URL.
  std::string error;
  LoadExtension(Manifest(manifest_value.get(), "override_files"),
                &error, Extension::COMPONENT, Extension::NO_FLAGS);
#if defined(FILE_MANAGER_EXTENSION)
  EXPECT_EQ("", error);
#else
  EXPECT_EQ(std::string(errors::kInvalidChromeURLOverrides), error);
#endif
}

}  // namespace
