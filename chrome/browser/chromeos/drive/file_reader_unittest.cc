// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_reader.h"

#include <string>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/threading/thread.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/google_apis/test_util.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace util {
namespace {

// An adapter to use test_util::ReadAllData.
class FileReaderAdapter {
 public:
  explicit FileReaderAdapter(FileReader* reader) : reader_(reader) {}
  int Read(net::IOBuffer* buffer,
           int buffer_length,
           const net::CompletionCallback& callback) {
    reader_->Read(buffer, buffer_length, callback);
    // As FileReader::Read always works asynchronously, return ERR_IO_PENDING.
    return net::ERR_IO_PENDING;
  }

 private:
  FileReader* reader_;
};

}  // namespace

class DriveFileReaderTest : public ::testing::Test {
 protected:
  DriveFileReaderTest() {
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    worker_thread_.reset(new base::Thread("DriveFileReaderTest"));
    ASSERT_TRUE(worker_thread_->Start());
    file_reader_.reset(new FileReader(worker_thread_->message_loop_proxy()));
  }

  virtual void TearDown() OVERRIDE {
    file_reader_.reset();
    worker_thread_.reset();
  }

  MessageLoop message_loop_;
  base::ScopedTempDir temp_dir_;
  scoped_ptr<base::Thread> worker_thread_;
  scoped_ptr<FileReader> file_reader_;
};

TEST_F(DriveFileReaderTest, NonExistingFile) {
  const base::FilePath kTestFile =
      temp_dir_.path().AppendASCII("non-existing");

  net::TestCompletionCallback callback;
  file_reader_->Open(kTestFile, 0, callback.callback());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, callback.WaitForResult());
}

TEST_F(DriveFileReaderTest, FullRead) {
  base::FilePath test_file;
  std::string expected_content;
  ASSERT_TRUE(google_apis::test_util::CreateFileOfSpecifiedSize(
      temp_dir_.path(), 1024, &test_file, &expected_content));

  net::TestCompletionCallback callback;
  file_reader_->Open(test_file, 0, callback.callback());
  ASSERT_EQ(net::OK, callback.WaitForResult());

  FileReaderAdapter adapter(file_reader_.get());
  std::string content;
  ASSERT_EQ(net::OK, test_util::ReadAllData(&adapter, &content));
  EXPECT_EQ(expected_content, content);
}

TEST_F(DriveFileReaderTest, OpenWithOffset) {
  base::FilePath test_file;
  std::string expected_content;
  ASSERT_TRUE(google_apis::test_util::CreateFileOfSpecifiedSize(
      temp_dir_.path(), 1024, &test_file, &expected_content));

  size_t offset = expected_content.size() / 2;
  expected_content.erase(0, offset);

  net::TestCompletionCallback callback;
  file_reader_->Open(
      test_file, static_cast<int64>(offset), callback.callback());
  ASSERT_EQ(net::OK, callback.WaitForResult());

  FileReaderAdapter adapter(file_reader_.get());
  std::string content;
  ASSERT_EQ(net::OK, test_util::ReadAllData(&adapter, &content));
  EXPECT_EQ(expected_content, content);
}

}  // namespace util
}  // namespace drive
