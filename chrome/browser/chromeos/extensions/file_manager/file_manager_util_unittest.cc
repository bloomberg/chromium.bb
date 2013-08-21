// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/file_manager_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace util {

TEST(FileManagerUtilTest, GetMimeTypeForPath) {
  EXPECT_EQ("image/jpeg",
            GetMimeTypeForPath(base::FilePath::FromUTF8Unsafe("foo.jpg")));
  EXPECT_EQ("image/jpeg",
            GetMimeTypeForPath(base::FilePath::FromUTF8Unsafe("FOO.JPG")));
  EXPECT_EQ("",
            GetMimeTypeForPath(base::FilePath::FromUTF8Unsafe("foo.zzz")));
}

}  // namespace util
}  // namespace file_manager
