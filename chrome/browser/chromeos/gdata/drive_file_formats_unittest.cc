// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/drive_file_formats.h"

#include "base/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gdata {

TEST(DriveFileFormatsTest, GetDriveFileFormat) {
  EXPECT_EQ(FILE_FORMAT_JPG, GetDriveFileFormat(FILE_PATH_LITERAL(".jpg")));
  EXPECT_EQ(FILE_FORMAT_JPG, GetDriveFileFormat(FILE_PATH_LITERAL(".JPG")));
  EXPECT_EQ(FILE_FORMAT_OTHER, GetDriveFileFormat(FILE_PATH_LITERAL(".XXX")));
}

}  // namespace gdata
