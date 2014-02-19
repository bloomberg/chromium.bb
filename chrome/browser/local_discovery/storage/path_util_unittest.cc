// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/storage/path_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_discovery {

TEST(PathUtilTest, ParsePrivetPath) {
  ParsedPrivetPath path(base::FilePath(FILE_PATH_LITERAL(
      "/privet/MyId._privet._tcp.local/Some name/some/path")));

  EXPECT_EQ("MyId._privet._tcp.local", path.service_name);
  EXPECT_EQ("/some/path", path.path);
}

TEST(PathUtilTest, ParseEmptyPath) {
  ParsedPrivetPath path(base::FilePath(FILE_PATH_LITERAL(
      "/privet/MyId._privet._tcp.local/Some name/")));

  EXPECT_EQ("MyId._privet._tcp.local", path.service_name);
  EXPECT_EQ("/", path.path);
}

}  // namespace local_discovery
