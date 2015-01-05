// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_handlers/app_file_handler_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace app_file_handler_util {
namespace {

FileHandlerInfo CreateHandlerInfoFromExtension(const std::string& extension) {
  FileHandlerInfo handler_info;
  handler_info.extensions.insert(extension);
  return handler_info;
}

}

TEST(FileHandlersAppFileHandlerUtilTest, FileHandlerCanHandleFile) {
  // File handler for extension "gz" should accept "*.gz", including "*.tar.gz".
  EXPECT_TRUE(FileHandlerCanHandleFile(
      CreateHandlerInfoFromExtension("gz"),
      "application/octet-stream",
      base::FilePath::FromUTF8Unsafe("foo.gz")));
  EXPECT_FALSE(FileHandlerCanHandleFile(
      CreateHandlerInfoFromExtension("gz"),
      "application/octet-stream",
      base::FilePath::FromUTF8Unsafe("foo.tgz")));
  EXPECT_TRUE(FileHandlerCanHandleFile(
      CreateHandlerInfoFromExtension("gz"),
      "application/octet-stream",
      base::FilePath::FromUTF8Unsafe("foo.tar.gz")));
  EXPECT_FALSE(FileHandlerCanHandleFile(
      CreateHandlerInfoFromExtension("tar.gz"),
      "application/octet-stream",
      base::FilePath::FromUTF8Unsafe("foo.gz")));
  EXPECT_TRUE(FileHandlerCanHandleFile(
      CreateHandlerInfoFromExtension("tar.gz"),
      "application/octet-stream",
      base::FilePath::FromUTF8Unsafe("foo.tar.gz")));
}

}  // namespace app_file_handler_util
}  // namespace extensions
