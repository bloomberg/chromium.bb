// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/mime_util.h"

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

TEST(FileManagerMimeUtilTest, GetMimeTypesForPaths) {
  const base::FilePath kFoo = base::FilePath::FromUTF8Unsafe("foo.jpg");
  const base::FilePath kBar = base::FilePath::FromUTF8Unsafe("BAR.JPG");
  const base::FilePath kBaz = base::FilePath::FromUTF8Unsafe("baz.zzz");
  std::vector<base::FilePath> file_paths;
  file_paths.push_back(kFoo);
  file_paths.push_back(kBar);
  file_paths.push_back(kBaz);

  extensions::app_file_handler_util::PathAndMimeTypeSet path_mime_set;
  GetMimeTypesForPaths(file_paths, &path_mime_set);

  EXPECT_EQ(3U, path_mime_set.size());
  EXPECT_EQ(1U, path_mime_set.count(std::make_pair(kFoo, "image/jpeg")));
  EXPECT_EQ(1U, path_mime_set.count(std::make_pair(kBar, "image/jpeg")));
  EXPECT_EQ(1U, path_mime_set.count(std::make_pair(kBaz, "")));
}

}  // namespace util
}  // namespace file_manager
