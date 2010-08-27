// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/file_system/file_system_host_context.h"
#include "chrome/browser/file_system/file_system_host_context.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileSystem.h"

namespace {

// PS stands for path separator.
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
#define PS  "\\"
#else
#define PS  "/"
#endif

const FilePath::CharType kTestDataPath[] = FILE_PATH_LITERAL(
        "//tmp/TestingProfilePath");

const struct RootPathTest {
  WebKit::WebFileSystem::Type type;
  bool off_the_record;
  const char* origin_url;
  bool expect_root_path;
  const char* expected_path;
} kRootPathTestCases[] = {
  { WebKit::WebFileSystem::TypeTemporary, false, "http://host:1/",
    true, "FileSystem" PS "http_host_1" PS "Temporary" },
  { WebKit::WebFileSystem::TypePersistent,  false, "http://host:2/",
    true, "FileSystem" PS "http_host_2" PS "Persistent" },
  { WebKit::WebFileSystem::TypeTemporary, true,  "http://host:3/",
    false, "" },
  { WebKit::WebFileSystem::TypePersistent,  true,  "http://host:4/",
    false, "" },
};

const struct CheckValidPathTest {
  FilePath::StringType path;
  bool expected_valid;
} kCheckValidPathTestCases[] = {
  { FILE_PATH_LITERAL("//tmp/foo.txt"), false, },
  { FILE_PATH_LITERAL("//etc/hosts"), false, },
  { FILE_PATH_LITERAL("foo.txt"), true, },
  { FILE_PATH_LITERAL("a/b/c"), true, },
  // Any paths that includes parent references are considered invalid.
  { FILE_PATH_LITERAL(".."), false, },
  { FILE_PATH_LITERAL("tmp/.."), false, },
  { FILE_PATH_LITERAL("a/b/../c/.."), false, },
};

}  // namespace

TEST(FileSystemHostContextTest, GetRootPath) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kRootPathTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "RootPath #" << i << " "
                 << kRootPathTestCases[i].expected_path);

    scoped_refptr<FileSystemHostContext> context(
        new FileSystemHostContext(FilePath(kTestDataPath),
                                  kRootPathTestCases[i].off_the_record));

    FilePath root_path;
    bool result = context->GetFileSystemRootPath(
            GURL(kRootPathTestCases[i].origin_url),
            kRootPathTestCases[i].type,
            &root_path, NULL);
    EXPECT_EQ(kRootPathTestCases[i].expect_root_path, result);
    if (result) {
      FilePath expected = FilePath(kTestDataPath).AppendASCII(
          kRootPathTestCases[i].expected_path);
      EXPECT_EQ(expected.value(), root_path.value());
    }
  }
}

TEST(FileSystemHostContextTest, CheckValidPath) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kCheckValidPathTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "CheckValidPath #" << i << " "
                 << kCheckValidPathTestCases[i].path);

    scoped_refptr<FileSystemHostContext> context(
        new FileSystemHostContext(FilePath(kTestDataPath), false));

    FilePath root_path;
    EXPECT_TRUE(context->GetFileSystemRootPath(
        GURL("http://foo.com/"), WebKit::WebFileSystem::TypePersistent,
        &root_path, NULL));
    FilePath path(kCheckValidPathTestCases[i].path);
    if (!path.IsAbsolute())
      path = root_path.Append(path);

    EXPECT_EQ(kCheckValidPathTestCases[i].expected_valid,
              context->CheckValidFileSystemPath(path));
  }
}
