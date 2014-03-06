// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/file_util.h"

#include "base/basictypes.h"
#include "base/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

typedef testing::Test FileUtilTest;

TEST_F(FileUtilTest, ExtensionURLToRelativeFilePath) {
#define URL_PREFIX "chrome-extension://extension-id/"
  struct TestCase {
    const char* url;
    const char* expected_relative_path;
  } test_cases[] = {
    { URL_PREFIX "simple.html",
      "simple.html" },
    { URL_PREFIX "directory/to/file.html",
      "directory/to/file.html" },
    { URL_PREFIX "escape%20spaces.html",
      "escape spaces.html" },
    { URL_PREFIX "%C3%9Cber.html",
      "\xC3\x9C" "ber.html" },
#if defined(OS_WIN)
    { URL_PREFIX "C%3A/simple.html",
      "" },
#endif
    { URL_PREFIX "////simple.html",
      "simple.html" },
    { URL_PREFIX "/simple.html",
      "simple.html" },
    { URL_PREFIX "\\simple.html",
      "simple.html" },
    { URL_PREFIX "\\\\foo\\simple.html",
      "foo/simple.html" },
  };
#undef URL_PREFIX

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    GURL url(test_cases[i].url);
    base::FilePath expected_path =
        base::FilePath::FromUTF8Unsafe(test_cases[i].expected_relative_path);
    base::FilePath actual_path =
        extensions::file_util::ExtensionURLToRelativeFilePath(url);
    EXPECT_FALSE(actual_path.IsAbsolute()) <<
      " For the path " << actual_path.value();
    EXPECT_EQ(expected_path.value(), actual_path.value()) <<
      " For the path " << url;
  }
}

TEST_F(FileUtilTest, ExtensionResourceURLToFilePath) {
  // Setup filesystem for testing.
  base::FilePath root_path;
  ASSERT_TRUE(base::CreateNewTempDirectory(
      base::FilePath::StringType(), &root_path));
  root_path = base::MakeAbsoluteFilePath(root_path);
  ASSERT_FALSE(root_path.empty());

  base::FilePath api_path = root_path.Append(FILE_PATH_LITERAL("apiname"));
  ASSERT_TRUE(base::CreateDirectory(api_path));

  const char data[] = "Test Data";
  base::FilePath resource_path = api_path.Append(FILE_PATH_LITERAL("test.js"));
  ASSERT_TRUE(base::WriteFile(resource_path, data, sizeof(data)));
  resource_path = api_path.Append(FILE_PATH_LITERAL("escape spaces.js"));
  ASSERT_TRUE(base::WriteFile(resource_path, data, sizeof(data)));

#ifdef FILE_PATH_USES_WIN_SEPARATORS
#define SEP "\\"
#else
#define SEP "/"
#endif
#define URL_PREFIX "chrome-extension-resource://"
  struct TestCase {
    const char* url;
    const base::FilePath::CharType* expected_path;
  } test_cases[] = {
    { URL_PREFIX "apiname/test.js",
      FILE_PATH_LITERAL("test.js") },
    { URL_PREFIX "/apiname/test.js",
      FILE_PATH_LITERAL("test.js") },
    // Test % escape
    { URL_PREFIX "apiname/%74%65st.js",
      FILE_PATH_LITERAL("test.js") },
    { URL_PREFIX "apiname/escape%20spaces.js",
      FILE_PATH_LITERAL("escape spaces.js") },
    // Test file does not exist.
    { URL_PREFIX "apiname/directory/to/file.js",
      NULL },
    // Test apiname/../../test.js
    { URL_PREFIX "apiname/../../test.js",
      FILE_PATH_LITERAL("test.js") },
    { URL_PREFIX "apiname/..%2F../test.js",
      NULL },
    { URL_PREFIX "apiname/f/../../../test.js",
      FILE_PATH_LITERAL("test.js") },
    { URL_PREFIX "apiname/f%2F..%2F..%2F../test.js",
      NULL },
  };
#undef SEP
#undef URL_PREFIX

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    GURL url(test_cases[i].url);
    base::FilePath expected_path;
    if (test_cases[i].expected_path)
      expected_path = root_path.Append(FILE_PATH_LITERAL("apiname")).Append(
          test_cases[i].expected_path);
    base::FilePath actual_path =
        extensions::file_util::ExtensionResourceURLToFilePath(url, root_path);
    EXPECT_EQ(expected_path.value(), actual_path.value()) <<
      " For the path " << url;
  }
  // Remove temp files.
  ASSERT_TRUE(base::DeleteFile(root_path, true));
}
