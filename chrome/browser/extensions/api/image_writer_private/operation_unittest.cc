// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/operation.h"
#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"
#include "chrome/browser/extensions/api/image_writer_private/test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/zip.h"

namespace extensions {
namespace image_writer {

namespace {

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::Gt;
using testing::Lt;

// This class gives us a generic Operation with the ability to set or inspect
// the current path to the image file.
class OperationForTest : public Operation {
 public:
  OperationForTest(base::WeakPtr<OperationManager> manager_,
                   const ExtensionId& extension_id,
                   const std::string& device_path)
      : Operation(manager_, extension_id, device_path) {}

  virtual void StartImpl() OVERRIDE {}

  // Expose internal stages for testing.
  void Unzip(const base::Closure& continuation) {
    Operation::Unzip(continuation);
  }

  void Write(const base::Closure& continuation) {
    Operation::Write(continuation);
  }

  void VerifyWrite(const base::Closure& continuation) {
    Operation::VerifyWrite(continuation);
  }

  // Helpers to set-up state for intermediate stages.
  void SetImagePath(const base::FilePath image_path) {
    image_path_ = image_path;
  }

  base::FilePath GetImagePath() { return image_path_; }

 private:
  virtual ~OperationForTest() {};
};

class ImageWriterOperationTest : public ImageWriterUnitTestBase {
 protected:
  virtual void SetUp() OVERRIDE {
    ImageWriterUnitTestBase::SetUp();

    // Create the zip file.
    base::FilePath image_dir = temp_dir_.path().AppendASCII("zip");
    ASSERT_TRUE(base::CreateDirectory(image_dir));
    ASSERT_TRUE(base::CreateTemporaryFileInDir(image_dir, &image_path_));

    FillFile(image_path_, kImagePattern, kTestFileSize);

    zip_file_ = temp_dir_.path().AppendASCII("test_image.zip");
    ASSERT_TRUE(zip::Zip(image_dir, zip_file_, true));
  }

  virtual void TearDown() OVERRIDE {
    ImageWriterUnitTestBase::TearDown();
  }

  base::FilePath image_path_;
  base::FilePath zip_file_;

  MockOperationManager manager_;
};

} // namespace

TEST_F(ImageWriterOperationTest, UnzipNonZipFile) {
  scoped_refptr<OperationForTest> operation(
      new OperationForTest(manager_.AsWeakPtr(),
                           kDummyExtensionId,
                           test_device_path_.AsUTF8Unsafe()));

  EXPECT_CALL(manager_, OnProgress(kDummyExtensionId, _, _)).Times(0);

  operation->SetImagePath(test_image_path_);

  operation->Start();
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &OperationForTest::Unzip, operation, base::Bind(&base::DoNothing)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ImageWriterOperationTest, UnzipZipFile) {
  scoped_refptr<OperationForTest> operation(
      new OperationForTest(manager_.AsWeakPtr(),
                           kDummyExtensionId,
                           test_device_path_.AsUTF8Unsafe()));

  EXPECT_CALL(manager_, OnError(kDummyExtensionId, _, _, _)).Times(0);
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::STAGE_UNZIP, _))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::STAGE_UNZIP, 0))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::STAGE_UNZIP, 100))
      .Times(AtLeast(1));

  operation->SetImagePath(zip_file_);

  operation->Start();
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &OperationForTest::Unzip, operation, base::Bind(&base::DoNothing)));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(base::ContentsEqual(image_path_, operation->GetImagePath()));
}

#if defined(OS_LINUX)
TEST_F(ImageWriterOperationTest, WriteImageToDevice) {

  scoped_refptr<OperationForTest> operation(
      new OperationForTest(manager_.AsWeakPtr(),
                           kDummyExtensionId,
                           test_device_path_.AsUTF8Unsafe()));

  EXPECT_CALL(manager_, OnError(kDummyExtensionId, _, _, _)).Times(0);
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::STAGE_WRITE, _))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::STAGE_WRITE, 0))
      .Times(AtLeast(1));
  EXPECT_CALL(manager_,
              OnProgress(kDummyExtensionId, image_writer_api::STAGE_WRITE, 100))
      .Times(AtLeast(1));

  operation->SetImagePath(test_image_path_);

  operation->Start();
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &OperationForTest::Write, operation, base::Bind(&base::DoNothing)));

  base::RunLoop().RunUntilIdle();

#if !defined(OS_CHROMEOS)
  // Chrome OS tests don't actually write to the disk because that's handled by
  // the DBUS process.
  EXPECT_TRUE(base::ContentsEqual(test_image_path_, test_device_path_));
#endif
}
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
// Chrome OS doesn't support verification in the ImageBurner, so these two tests
// are skipped.

TEST_F(ImageWriterOperationTest, VerifyFileSuccess) {
  scoped_refptr<OperationForTest> operation(
      new OperationForTest(manager_.AsWeakPtr(),
                           kDummyExtensionId,
                           test_device_path_.AsUTF8Unsafe()));

  EXPECT_CALL(manager_, OnError(kDummyExtensionId, _, _, _)).Times(0);
  EXPECT_CALL(
      manager_,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_VERIFYWRITE, _))
      .Times(AtLeast(1));
  EXPECT_CALL(
      manager_,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_VERIFYWRITE, 0))
      .Times(AtLeast(1));
  EXPECT_CALL(
      manager_,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_VERIFYWRITE, 100))
      .Times(AtLeast(1));

  FillFile(test_device_path_, kImagePattern, kTestFileSize);
  operation->SetImagePath(test_image_path_);

  operation->Start();
  content::BrowserThread::PostTask(content::BrowserThread::FILE,
                                   FROM_HERE,
                                   base::Bind(&OperationForTest::VerifyWrite,
                                              operation,
                                              base::Bind(&base::DoNothing)));

  base::RunLoop().RunUntilIdle();
}

TEST_F(ImageWriterOperationTest, VerifyFileFailure) {
  scoped_refptr<OperationForTest> operation(
      new OperationForTest(manager_.AsWeakPtr(),
                           kDummyExtensionId,
                           test_device_path_.AsUTF8Unsafe()));

  EXPECT_CALL(
      manager_,
      OnError(kDummyExtensionId, image_writer_api::STAGE_VERIFYWRITE, _, _))
      .Times(1);
  EXPECT_CALL(
      manager_,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_VERIFYWRITE, _))
      .Times(AnyNumber());
  EXPECT_CALL(
      manager_,
      OnProgress(kDummyExtensionId, image_writer_api::STAGE_VERIFYWRITE, 100))
      .Times(0);

  FillFile(test_device_path_, kDevicePattern, kTestFileSize);
  operation->SetImagePath(test_image_path_);

  operation->Start();
  content::BrowserThread::PostTask(content::BrowserThread::FILE,
                                   FROM_HERE,
                                   base::Bind(&OperationForTest::VerifyWrite,
                                              operation,
                                              base::Bind(&base::DoNothing)));

  base::RunLoop().RunUntilIdle();
}
#endif

// Tests that on creation the operation has the expected state.
TEST_F(ImageWriterOperationTest, Creation) {
  scoped_refptr<Operation> op(
      new OperationForTest(manager_.AsWeakPtr(),
                           kDummyExtensionId,
                           test_device_path_.AsUTF8Unsafe()));

  EXPECT_EQ(0, op->GetProgress());
  EXPECT_EQ(image_writer_api::STAGE_UNKNOWN, op->GetStage());
}

}  // namespace image_writer
}  // namespace extensions
