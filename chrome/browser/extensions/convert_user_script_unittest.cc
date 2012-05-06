// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/convert_user_script.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

static void AddPattern(URLPatternSet* extent, const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  extent->AddPattern(URLPattern(schemes, pattern));
}

}

TEST(ExtensionFromUserScript, Basic) {
  FilePath test_file;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_file));
  test_file = test_file.AppendASCII("extensions")
                       .AppendASCII("user_script_basic.user.js");

  string16 error;
  scoped_refptr<Extension> extension(ConvertUserScriptToExtension(
      test_file, GURL("http://www.google.com/foo"), &error));

  ASSERT_TRUE(extension.get());
  EXPECT_EQ(string16(), error);

  // Use a temp dir so that the extensions dir will clean itself up.
  ScopedTempDir ext_dir;
  EXPECT_TRUE(ext_dir.Set(extension->path()));

  // Validate generated extension metadata.
  EXPECT_EQ("My user script", extension->name());
  EXPECT_EQ("2.2.2", extension->VersionString());
  EXPECT_EQ("Does totally awesome stuff.", extension->description());
  EXPECT_EQ("IhCFCg9PMQTAcJdc9ytUP99WME+4yh6aMnM1uupkovo=",
            extension->public_key());

  ASSERT_EQ(1u, extension->content_scripts().size());
  const UserScript& script = extension->content_scripts()[0];
  EXPECT_EQ(UserScript::DOCUMENT_IDLE, script.run_location());
  ASSERT_EQ(2u, script.globs().size());
  EXPECT_EQ("http://www.google.com/*", script.globs().at(0));
  EXPECT_EQ("http://www.yahoo.com/*", script.globs().at(1));
  ASSERT_EQ(1u, script.exclude_globs().size());
  EXPECT_EQ("*foo*", script.exclude_globs().at(0));
  ASSERT_EQ(1u, script.url_patterns().patterns().size());
  EXPECT_EQ("http://www.google.com/*",
            script.url_patterns().begin()->GetAsString());
  ASSERT_EQ(1u, script.exclude_url_patterns().patterns().size());
  EXPECT_EQ("http://www.google.com/foo*",
            script.exclude_url_patterns().begin()->GetAsString());

  // Make sure the files actually exist on disk.
  EXPECT_TRUE(file_util::PathExists(
      extension->path().Append(script.js_scripts()[0].relative_path())));
  EXPECT_TRUE(file_util::PathExists(
      extension->path().Append(Extension::kManifestFilename)));
}

TEST(ExtensionFromUserScript, NoMetdata) {
  FilePath test_file;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_file));
  test_file = test_file.AppendASCII("extensions")
                       .AppendASCII("user_script_no_metadata.user.js");

  string16 error;
  scoped_refptr<Extension> extension(ConvertUserScriptToExtension(
      test_file, GURL("http://www.google.com/foo/bar.user.js?monkey"), &error));

  ASSERT_TRUE(extension.get());
  EXPECT_EQ(string16(), error);

  // Use a temp dir so that the extensions dir will clean itself up.
  ScopedTempDir ext_dir;
  EXPECT_TRUE(ext_dir.Set(extension->path()));

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

  URLPatternSet expected;
  AddPattern(&expected, "http://*/*");
  AddPattern(&expected, "https://*/*");
  EXPECT_EQ(expected, script.url_patterns());

  // Make sure the files actually exist on disk.
  EXPECT_TRUE(file_util::PathExists(
      extension->path().Append(script.js_scripts()[0].relative_path())));
  EXPECT_TRUE(file_util::PathExists(
      extension->path().Append(Extension::kManifestFilename)));
}

TEST(ExtensionFromUserScript, NotUTF8) {
  FilePath test_file;

  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_file));
  test_file = test_file.AppendASCII("extensions")
                       .AppendASCII("user_script_not_utf8.user.js");

  string16 error;
  scoped_refptr<Extension> extension(ConvertUserScriptToExtension(
      test_file, GURL("http://www.google.com/foo/bar.user.js?monkey"), &error));

  ASSERT_FALSE(extension.get());
  EXPECT_EQ(ASCIIToUTF16("User script must be UTF8 encoded."), error);
}

TEST(ExtensionFromUserScript, RunAtDocumentStart) {
  FilePath test_file;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_file));
  test_file = test_file.AppendASCII("extensions")
                       .AppendASCII("user_script_run_at_start.user.js");

  string16 error;
  scoped_refptr<Extension> extension(ConvertUserScriptToExtension(
      test_file, GURL("http://www.google.com/foo"), &error));

  ASSERT_TRUE(extension.get());
  EXPECT_EQ(string16(), error);

  // Use a temp dir so that the extensions dir will clean itself up.
  ScopedTempDir ext_dir;
  EXPECT_TRUE(ext_dir.Set(extension->path()));

  // Validate generated extension metadata.
  EXPECT_EQ("Document Start Test", extension->name());
  EXPECT_EQ("This script tests document-start", extension->description());
  EXPECT_EQ("RjmyI7+Gp/YHcW1qnu4xDxkJcL4cV4kTzdCA4BajCbk=",
            extension->public_key());

  // Validate run location.
  ASSERT_EQ(1u, extension->content_scripts().size());
  const UserScript& script = extension->content_scripts()[0];
  EXPECT_EQ(UserScript::DOCUMENT_START, script.run_location());
}

TEST(ExtensionFromUserScript, RunAtDocumentEnd) {
  FilePath test_file;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_file));
  test_file = test_file.AppendASCII("extensions")
                       .AppendASCII("user_script_run_at_end.user.js");

  string16 error;
  scoped_refptr<Extension> extension(ConvertUserScriptToExtension(
      test_file, GURL("http://www.google.com/foo"), &error));

  ASSERT_TRUE(extension.get());
  EXPECT_EQ(string16(), error);

  // Use a temp dir so that the extensions dir will clean itself up.
  ScopedTempDir ext_dir;
  EXPECT_TRUE(ext_dir.Set(extension->path()));

  // Validate generated extension metadata.
  EXPECT_EQ("Document End Test", extension->name());
  EXPECT_EQ("This script tests document-end", extension->description());
  EXPECT_EQ("cpr5i8Mi24FzECV8UJe6tanwlU8SWesZosJ915YISvQ=",
            extension->public_key());

  // Validate run location.
  ASSERT_EQ(1u, extension->content_scripts().size());
  const UserScript& script = extension->content_scripts()[0];
  EXPECT_EQ(UserScript::DOCUMENT_END, script.run_location());
}

TEST(ExtensionFromUserScript, RunAtDocumentIdle) {
  FilePath test_file;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_file));
  test_file = test_file.AppendASCII("extensions")
                       .AppendASCII("user_script_run_at_idle.user.js");
  ASSERT_TRUE(file_util::PathExists(test_file)) << test_file.value();

  string16 error;
  scoped_refptr<Extension> extension(ConvertUserScriptToExtension(
      test_file, GURL("http://www.google.com/foo"), &error));

  ASSERT_TRUE(extension.get());
  EXPECT_EQ(string16(), error);

  // Use a temp dir so that the extensions dir will clean itself up.
  ScopedTempDir ext_dir;
  EXPECT_TRUE(ext_dir.Set(extension->path()));

  // Validate generated extension metadata.
  EXPECT_EQ("Document Idle Test", extension->name());
  EXPECT_EQ("This script tests document-idle", extension->description());
  EXPECT_EQ("kHnHKec3O/RKKo5/Iu1hKqe4wQERthL0639isNtsfiY=",
            extension->public_key());

  // Validate run location.
  ASSERT_EQ(1u, extension->content_scripts().size());
  const UserScript& script = extension->content_scripts()[0];
  EXPECT_EQ(UserScript::DOCUMENT_IDLE, script.run_location());
}
