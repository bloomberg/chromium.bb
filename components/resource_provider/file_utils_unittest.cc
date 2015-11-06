// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/resource_provider/file_utils.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace resource_provider {

// Assertions for invalid app paths.
TEST(FileUtilsTest, InvalidAppPath) {
  struct TestCase {
    std::string url;
  };
  struct TestCase invalid_cases[]{
      {"http://foo"},     // Must start with 'mojo:'.
      {"mojo://."},       // Don't allow '.'.
      {"mojo://.."},      // Don't allow '..'.
      {"mojo://foo/."},   // Don't allow '.'.
      {"mojo://bar/.."},  // Don't allow '..'.
  };

  for (size_t i = 0; i < arraysize(invalid_cases); ++i) {
    base::FilePath resulting_path(GetPathForApplicationUrl(
        invalid_cases[i].url));
    EXPECT_TRUE(resulting_path.empty()) << "i=" << i
                                        << " input=" << invalid_cases[i].url
                                        << " result=" << resulting_path.value();
  }
}

// Assertions for invalid app paths.
TEST(FileUtilsTest, InvalidResourcePath) {
  struct TestCase {
    std::string path;
  };
  struct TestCase invalid_cases[]{
      {"."},
      {".."},
      {"foo/."},
      {"bar/.."},
      {"foo/./bar"},
      {"bar/../baz"},
      {"bar/baz/"},
      {"bar//baz/"},
  };

  const base::FilePath app_path(GetPathForApplicationUrl("mojo:test"));
  ASSERT_FALSE(app_path.empty());

  for (size_t i = 0; i < arraysize(invalid_cases); ++i) {
    base::FilePath resulting_path(
        GetPathForResourceNamed(app_path, invalid_cases[i].path));
    EXPECT_TRUE(resulting_path.empty()) << i
                                        << " input=" << invalid_cases[i].path
                                        << " result=" << resulting_path.value();
  }
}

TEST(FileUtilsTest, ValidPaths) {
  const base::FilePath app_path(GetPathForApplicationUrl("mojo:test"));
  ASSERT_FALSE(app_path.empty());

  // Trivial single path element.
  const base::FilePath trivial_path(
      GetPathForResourceNamed(app_path, "single"));
  EXPECT_EQ(app_path.AppendASCII("single").value(), trivial_path.value());

  // Two path elements.
  const base::FilePath two_paths(GetPathForResourceNamed(app_path, "a/b"));
  EXPECT_EQ(app_path.AppendASCII("a").AppendASCII("b").value(),
            two_paths.value());
}

}  // namespace resource_provider
