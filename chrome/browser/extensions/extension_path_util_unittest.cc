// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/path_util.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::FilePath;

namespace extensions {

// Basic unittest for path_util::PrettifyPath in
// chrome/browser/extensions/path_util.cc.
// For legacy reasons, it's tested more in
// FileSystemApiTest.FileSystemApiGetDisplayPathPrettify.
TEST(ExtensionPathUtilTest, BasicPrettifyPathTest) {
  const FilePath::CharType kHomeShortcut[] = FILE_PATH_LITERAL("~");

  // Test prettifying empty path.
  FilePath unprettified;
  FilePath prettified = path_util::PrettifyPath(unprettified);
  EXPECT_EQ(unprettified, prettified);

  // Test home directory ("~").
  unprettified = base::GetHomeDir();
  prettified = path_util::PrettifyPath(unprettified);
  EXPECT_NE(unprettified, prettified);
  EXPECT_EQ(FilePath(kHomeShortcut), prettified);

  // Test with one layer ("~/foo").
  unprettified = unprettified.AppendASCII("foo");
  prettified = path_util::PrettifyPath(unprettified);
  EXPECT_NE(unprettified, prettified);
  EXPECT_EQ(FilePath(kHomeShortcut).AppendASCII("foo"), prettified);

  // Test with two layers ("~/foo/bar").
  unprettified = unprettified.AppendASCII("bar");
  prettified = path_util::PrettifyPath(unprettified);
  EXPECT_NE(unprettified, prettified);
  EXPECT_EQ(
      FilePath(kHomeShortcut).AppendASCII("foo").AppendASCII("bar"),
      prettified);
}

}  // namespace extensions
