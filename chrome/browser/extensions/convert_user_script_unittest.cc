// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/convert_user_script.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ExtensionFromUserScript, Basic) {
  FilePath test_file;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_file));
  test_file = test_file.AppendASCII("extensions")
                       .AppendASCII("user_script_basic.user.js");

  std::string error;
  scoped_refptr<Extension> extension(ConvertUserScriptToExtension(
      test_file, GURL("http://www.google.com/foo"), &error));

  ASSERT_TRUE(extension.get());
  EXPECT_EQ("", error);

  // Validate generated extension metadata.
  EXPECT_EQ("My user script", extension->name());
  EXPECT_EQ("2.2.2", extension->VersionString());
  EXPECT_EQ("Does totally awesome stuff.", extension->description());
  EXPECT_EQ("IhCFCg9PMQTAcJdc9ytUP99WME+4yh6aMnM1uupkovo=",
            extension->public_key());

  ASSERT_EQ(1u, extension->content_scripts().size());
  const UserScript& script = extension->content_scripts()[0];
  ASSERT_EQ(2u, script.globs().size());
  EXPECT_EQ("http://www.google.com/*", script.globs().at(0));
  EXPECT_EQ("http://www.yahoo.com/*", script.globs().at(1));
  ASSERT_EQ(1u, script.exclude_globs().size());
  EXPECT_EQ("*foo*", script.exclude_globs().at(0));
  ASSERT_EQ(1u, script.url_patterns().size());
  EXPECT_EQ("http://www.google.com/*", script.url_patterns()[0].GetAsString());

  // Make sure the files actually exist on disk.
  EXPECT_TRUE(file_util::PathExists(
      extension->path().Append(script.js_scripts()[0].relative_path())));
  EXPECT_TRUE(file_util::PathExists(
      extension->path().Append(Extension::kManifestFilename)));

  // Cleanup
  file_util::Delete(extension->path(), true);
}

TEST(ExtensionFromUserScript, NoMetdata) {
  FilePath test_file;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_file));
  test_file = test_file.AppendASCII("extensions")
                       .AppendASCII("user_script_no_metadata.user.js");

  std::string error;
  scoped_refptr<Extension> extension(ConvertUserScriptToExtension(
      test_file, GURL("http://www.google.com/foo/bar.user.js?monkey"), &error));

  ASSERT_TRUE(extension.get());
  EXPECT_EQ("", error);

  // Validate generated extension metadata.
  EXPECT_EQ("bar.user.js", extension->name());
  EXPECT_EQ("1.0", extension->VersionString());
  EXPECT_EQ("", extension->description());
  EXPECT_EQ("k1WxKx54hX6tfl5gQaXD/m4d9QUMwRdXWM4RW+QkWcY=",
            extension->public_key());

  ASSERT_EQ(1u, extension->content_scripts().size());
  const UserScript& script = extension->content_scripts()[0];
  ASSERT_EQ(1u, script.globs().size());
  EXPECT_EQ("*", script.globs()[0]);
  EXPECT_EQ(0u, script.exclude_globs().size());
  ASSERT_EQ(2u, script.url_patterns().size());
  EXPECT_EQ("http://*/*", script.url_patterns()[0].GetAsString());
  EXPECT_EQ("https://*/*", script.url_patterns()[1].GetAsString());

  // Make sure the files actually exist on disk.
  EXPECT_TRUE(file_util::PathExists(
      extension->path().Append(script.js_scripts()[0].relative_path())));
  EXPECT_TRUE(file_util::PathExists(
      extension->path().Append(Extension::kManifestFilename)));

  // Cleanup
  file_util::Delete(extension->path(), true);
}

TEST(ExtensionFromUserScript, NotUTF8) {
  FilePath test_file;

  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_file));
  test_file = test_file.AppendASCII("extensions")
                       .AppendASCII("user_script_not_utf8.user.js");

  std::string error;
  scoped_refptr<Extension> extension(ConvertUserScriptToExtension(
      test_file, GURL("http://www.google.com/foo/bar.user.js?monkey"), &error));

  ASSERT_FALSE(extension.get());
  EXPECT_EQ("User script must be UTF8 encoded.", error);
}
