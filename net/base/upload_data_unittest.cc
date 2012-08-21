// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/upload_data.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

// Simplified version of TestCompletionCallback for ContentLengthCallback,
// that handles uint64 rather than int.
class TestContentLengthCallback {
 public:
  TestContentLengthCallback()
      : result_(0),
        callback_(ALLOW_THIS_IN_INITIALIZER_LIST(
            base::Bind(&TestContentLengthCallback::SetResult,
                       base::Unretained(this)))) {
  }

  ~TestContentLengthCallback() {}

  const UploadData::ContentLengthCallback& callback() const {
    return callback_;
  }

  // Waits for the result and returns it.
  uint64 WaitForResult() {
    MessageLoop::current()->Run();
    return result_;
  }

 private:
  // Sets the result and stops the message loop.
  void SetResult(uint64 result) {
    result_ = result;
    MessageLoop::current()->Quit();
  }

  uint64 result_;
  const UploadData::ContentLengthCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(TestContentLengthCallback);
};

class UploadDataTest : public PlatformTest {
 protected:
  virtual void SetUp() {
    upload_data_= new UploadData;
    // To ensure that file IO is not performed on the main thread in the
    // test (i.e. GetContentLengthSync() will fail if file IO is performed).
    base::ThreadRestrictions::SetIOAllowed(false);
  }

  virtual void TearDown() {
    base::ThreadRestrictions::SetIOAllowed(true);
  }

  // Creates a temporary file with the given data. The temporary file is
  // deleted by temp_dir_. Returns true on success.
  bool CreateTemporaryFile(const std::string& data,
                           FilePath* temp_file_path) {
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    if (!temp_dir_.CreateUniqueTempDir())
      return false;
    if (!file_util::CreateTemporaryFileInDir(temp_dir_.path(), temp_file_path))
      return false;
    if (static_cast<int>(data.size()) !=
        file_util::WriteFile(*temp_file_path, data.data(), data.size()))
      return false;

    return true;
  }

  ScopedTempDir temp_dir_;
  scoped_refptr<UploadData> upload_data_;
};

TEST_F(UploadDataTest, IsInMemory_Empty) {
  ASSERT_TRUE(upload_data_->IsInMemory());
}

TEST_F(UploadDataTest, IsInMemory_Bytes) {
  upload_data_->AppendBytes("123", 3);
  ASSERT_TRUE(upload_data_->IsInMemory());
}

TEST_F(UploadDataTest, IsInMemory_File) {
  upload_data_->AppendFileRange(
      FilePath(FILE_PATH_LITERAL("random_file_name.txt")),
      0, 0, base::Time());
  ASSERT_FALSE(upload_data_->IsInMemory());
}

TEST_F(UploadDataTest, IsInMemory_Blob) {
  upload_data_->AppendBlob(GURL("blog:internal:12345"));
  // Until it's resolved, we don't know what blob contains.
  ASSERT_FALSE(upload_data_->IsInMemory());
}

TEST_F(UploadDataTest, IsInMemory_Chunk) {
  upload_data_->set_is_chunked(true);
  ASSERT_FALSE(upload_data_->IsInMemory());
}

TEST_F(UploadDataTest, IsInMemory_Mixed) {
  ASSERT_TRUE(upload_data_->IsInMemory());

  upload_data_->AppendBytes("123", 3);
  upload_data_->AppendBytes("abc", 3);
  ASSERT_TRUE(upload_data_->IsInMemory());

  upload_data_->AppendFileRange(
      FilePath(FILE_PATH_LITERAL("random_file_name.txt")),
      0, 0, base::Time());
  ASSERT_FALSE(upload_data_->IsInMemory());
}

TEST_F(UploadDataTest, GetContentLength_Empty) {
  ASSERT_EQ(0U, upload_data_->GetContentLengthSync());
}

TEST_F(UploadDataTest, GetContentLength_Bytes) {
  upload_data_->AppendBytes("123", 3);
  ASSERT_EQ(3U, upload_data_->GetContentLengthSync());
}

TEST_F(UploadDataTest, GetContentLength_File) {
  // Create a temporary file with some data.
  const std::string kData = "hello";
  FilePath temp_file_path;
  ASSERT_TRUE(CreateTemporaryFile(kData, &temp_file_path));
  upload_data_->AppendFileRange(temp_file_path, 0, kuint64max, base::Time());

  // The length is returned asynchronously.
  TestContentLengthCallback callback;
  upload_data_->GetContentLength(callback.callback());
  ASSERT_EQ(kData.size(), callback.WaitForResult());
}

TEST_F(UploadDataTest, GetContentLength_Blob) {
  upload_data_->AppendBlob(GURL("blog:internal:12345"));
  ASSERT_EQ(0U, upload_data_->GetContentLengthSync());
}

TEST_F(UploadDataTest, GetContentLength_Chunk) {
  upload_data_->set_is_chunked(true);
  ASSERT_EQ(0U, upload_data_->GetContentLengthSync());
}

TEST_F(UploadDataTest, GetContentLength_Mixed) {
  upload_data_->AppendBytes("123", 3);
  upload_data_->AppendBytes("abc", 3);

  const uint64 content_length = upload_data_->GetContentLengthSync();
  ASSERT_EQ(6U, content_length);

  // Append a file.
  const std::string kData = "hello";
  FilePath temp_file_path;
  ASSERT_TRUE(CreateTemporaryFile(kData, &temp_file_path));
  upload_data_->AppendFileRange(temp_file_path, 0, kuint64max, base::Time());

  TestContentLengthCallback callback;
  upload_data_->GetContentLength(callback.callback());
  ASSERT_EQ(content_length + kData.size(), callback.WaitForResult());
}

}  // namespace net
