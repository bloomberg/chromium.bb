// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "chrome/utility/image_writer/error_messages.h"
#include "chrome/utility/image_writer/image_writer.h"
#include "chrome/utility/image_writer/image_writer_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace image_writer {

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::Lt;

namespace {

const int64 kTestFileSize = 1 << 15;  // 32 kB
const int kTestPattern = 0x55555555;

class ImageWriterUtilityTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.path(), &image_path_));
    ASSERT_TRUE(
        base::CreateTemporaryFileInDir(temp_dir_.path(), &device_path_));
  }

  virtual void TearDown() OVERRIDE {}

  void FillFile(const base::FilePath& path) {
    scoped_ptr<char[]> buffer(new char[kTestFileSize]);
    memset(buffer.get(), kTestPattern, kTestFileSize);

    ASSERT_TRUE(file_util::WriteFile(path, buffer.get(), kTestFileSize));
  }

  base::FilePath image_path_;
  base::FilePath device_path_;

 private:
  base::MessageLoop message_loop_;
  base::ScopedTempDir temp_dir_;
};

class MockHandler : public ImageWriterHandler {
 public:
  MOCK_METHOD1(SendProgress, void(int64));
  MOCK_METHOD1(SendFailed, void(const std::string& message));
  MOCK_METHOD0(SendSucceeded, void());
  MOCK_METHOD1(OnMessageReceived, bool(const IPC::Message& message));
};

}  // namespace

TEST_F(ImageWriterUtilityTest, WriteSuccessful) {
  MockHandler mock_handler;
  ImageWriter image_writer(&mock_handler);

  EXPECT_CALL(mock_handler, SendProgress(_)).Times(AnyNumber());
  EXPECT_CALL(mock_handler, SendProgress(kTestFileSize)).Times(1);
  EXPECT_CALL(mock_handler, SendProgress(0)).Times(1);
  EXPECT_CALL(mock_handler, SendSucceeded()).Times(1);
  EXPECT_CALL(mock_handler, SendFailed(_)).Times(0);

  FillFile(image_path_);
  image_writer.Write(image_path_, device_path_);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ImageWriterUtilityTest, WriteInvalidImageFile) {
  MockHandler mock_handler;
  ImageWriter image_writer(&mock_handler);

  EXPECT_CALL(mock_handler, SendProgress(_)).Times(0);
  EXPECT_CALL(mock_handler, SendSucceeded()).Times(0);
  EXPECT_CALL(mock_handler, SendFailed(error::kOpenImage)).Times(1);

  ASSERT_TRUE(base::DeleteFile(image_path_, false));
  image_writer.Write(image_path_, device_path_);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ImageWriterUtilityTest, WriteInvalidDeviceFile) {
  MockHandler mock_handler;
  ImageWriter image_writer(&mock_handler);

  EXPECT_CALL(mock_handler, SendProgress(_)).Times(0);
  EXPECT_CALL(mock_handler, SendSucceeded()).Times(0);
  EXPECT_CALL(mock_handler, SendFailed(error::kOpenDevice)).Times(1);

  ASSERT_TRUE(base::DeleteFile(device_path_, false));
  image_writer.Write(image_path_, device_path_);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ImageWriterUtilityTest, VerifySuccessful) {
  MockHandler mock_handler;
  ImageWriter image_writer(&mock_handler);

  EXPECT_CALL(mock_handler, SendProgress(_)).Times(AnyNumber());
  EXPECT_CALL(mock_handler, SendProgress(kTestFileSize)).Times(1);
  EXPECT_CALL(mock_handler, SendProgress(0)).Times(1);
  EXPECT_CALL(mock_handler, SendSucceeded()).Times(1);
  EXPECT_CALL(mock_handler, SendFailed(_)).Times(0);

  FillFile(image_path_);
  FillFile(device_path_);

  image_writer.Verify(image_path_, device_path_);

  base::RunLoop().RunUntilIdle();
}

TEST_F(ImageWriterUtilityTest, VerifyInvalidImageFile) {
  MockHandler mock_handler;
  ImageWriter image_writer(&mock_handler);

  EXPECT_CALL(mock_handler, SendProgress(_)).Times(0);
  EXPECT_CALL(mock_handler, SendSucceeded()).Times(0);
  EXPECT_CALL(mock_handler, SendFailed(error::kOpenImage)).Times(1);

  ASSERT_TRUE(base::DeleteFile(image_path_, false));

  image_writer.Verify(image_path_, device_path_);

  base::RunLoop().RunUntilIdle();
}

TEST_F(ImageWriterUtilityTest, VerifyInvalidDeviceFile) {
  MockHandler mock_handler;
  ImageWriter image_writer(&mock_handler);

  EXPECT_CALL(mock_handler, SendProgress(_)).Times(0);
  EXPECT_CALL(mock_handler, SendSucceeded()).Times(0);
  EXPECT_CALL(mock_handler, SendFailed(error::kOpenDevice)).Times(1);

  ASSERT_TRUE(base::DeleteFile(device_path_, false));

  image_writer.Verify(image_path_, device_path_);

  base::RunLoop().RunUntilIdle();
}

TEST_F(ImageWriterUtilityTest, VerifyFailed) {
  MockHandler mock_handler;
  ImageWriter image_writer(&mock_handler);

  EXPECT_CALL(mock_handler, SendProgress(_)).Times(AnyNumber());
  EXPECT_CALL(mock_handler, SendSucceeded()).Times(0);
  EXPECT_CALL(mock_handler, SendFailed(error::kVerificationFailed)).Times(1);

  FillFile(image_path_);
  image_writer.Verify(image_path_, device_path_);

  base::RunLoop().RunUntilIdle();
}

TEST_F(ImageWriterUtilityTest, WriteThenVerify) {
  MockHandler mock_handler;
  ImageWriter image_writer(&mock_handler);

  EXPECT_CALL(mock_handler, SendProgress(_)).Times(AnyNumber());
  EXPECT_CALL(mock_handler, SendSucceeded()).Times(2);
  EXPECT_CALL(mock_handler, SendFailed(_)).Times(0);

  image_writer.Write(image_path_, device_path_);

  base::RunLoop().RunUntilIdle();

  image_writer.Verify(image_path_, device_path_);

  base::RunLoop().RunUntilIdle();
}

}  // namespace image_writer
