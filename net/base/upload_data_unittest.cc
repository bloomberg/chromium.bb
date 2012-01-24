// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_data.h"

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(UploadDataTest, IsInMemory_Empty) {
  scoped_refptr<UploadData> upload_data = new UploadData;
  ASSERT_TRUE(upload_data->IsInMemory());
}

TEST(UploadDataTest, IsInMemory_Bytes) {
  scoped_refptr<UploadData> upload_data = new UploadData;
  upload_data->AppendBytes("123", 3);
  ASSERT_TRUE(upload_data->IsInMemory());
}

TEST(UploadDataTest, IsInMemory_File) {
  scoped_refptr<UploadData> upload_data = new UploadData;
  upload_data->AppendFileRange(
      FilePath(FILE_PATH_LITERAL("random_file_name.txt")),
      0, 0, base::Time());
  ASSERT_FALSE(upload_data->IsInMemory());
}

TEST(UploadDataTest, IsInMemory_Blob) {
  scoped_refptr<UploadData> upload_data = new UploadData;
  upload_data->AppendBlob(GURL("blog:internal:12345"));
  // Until it's resolved, we don't know what blob contains.
  ASSERT_FALSE(upload_data->IsInMemory());
}

TEST(UploadDataTest, IsInMemory_Chunk) {
  scoped_refptr<UploadData> upload_data = new UploadData;
  upload_data->set_is_chunked(true);
  ASSERT_FALSE(upload_data->IsInMemory());
}

TEST(UploadDataTest, IsInMemory_Mixed) {
  scoped_refptr<UploadData> upload_data = new UploadData;
  ASSERT_TRUE(upload_data->IsInMemory());

  upload_data->AppendBytes("123", 3);
  upload_data->AppendBytes("abc", 3);
  ASSERT_TRUE(upload_data->IsInMemory());

  upload_data->AppendFileRange(
      FilePath(FILE_PATH_LITERAL("random_file_name.txt")),
      0, 0, base::Time());
  ASSERT_FALSE(upload_data->IsInMemory());
}

}  // namespace net
