// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Set start and end ranges and content length of |upload_file_info|.
void SetRange(gdata::UploadFileInfo* upload_file_info,
              int64 start_range, int64 end_range, int64 content_length) {
  upload_file_info->start_range = start_range;
  upload_file_info->end_range = end_range;
  upload_file_info->content_length = content_length;
}

}  // namespace

TEST(GDataUploadFileInfoTest, GetContentRangeHeader) {
  gdata::UploadFileInfo upload_file_info;

  SetRange(&upload_file_info, 123456, 234567, 345678);
  EXPECT_EQ("Content-Range: bytes 123456-234567/345678",
            upload_file_info.GetContentRangeHeader());

  SetRange(&upload_file_info, 1234567890123, 2345678901234, 3456789012345);
  EXPECT_EQ("Content-Range: bytes 1234567890123-2345678901234/3456789012345",
            upload_file_info.GetContentRangeHeader());

  SetRange(&upload_file_info, 0, 512, -1);
  EXPECT_EQ("Content-Range: bytes 0-512/*",
            upload_file_info.GetContentRangeHeader());
}

TEST(GDataUploadFileInfoTest, GetContentTypeAndLengthHeaders) {
  gdata::UploadFileInfo upload_file_info;
  upload_file_info.content_type = "image/jpeg";
  upload_file_info.content_length = 3456789012345;

  std::vector<std::string> headers =
      upload_file_info.GetContentTypeAndLengthHeaders();
  ASSERT_EQ(2u, headers.size());
  EXPECT_EQ("X-Upload-Content-Type: image/jpeg", headers[0]);
  EXPECT_EQ("X-Upload-Content-Length: 3456789012345", headers[1]);
}
