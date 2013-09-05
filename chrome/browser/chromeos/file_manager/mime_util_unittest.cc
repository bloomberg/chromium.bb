// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/mime_util.h"

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace util {

TEST(FileManagerMimeUtilTest, GetMimeTypeForPath) {
  EXPECT_EQ("image/jpeg",
            GetMimeTypeForPath(base::FilePath::FromUTF8Unsafe("foo.jpg")));
  EXPECT_EQ("image/jpeg",
            GetMimeTypeForPath(base::FilePath::FromUTF8Unsafe("FOO.JPG")));
  EXPECT_EQ("",
            GetMimeTypeForPath(base::FilePath::FromUTF8Unsafe("foo.zzz")));
}

}  // namespace util
}  // namespace file_manager
