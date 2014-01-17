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

// This class gives us access to the protected methods of Operation so that we
// can call them directly.  It also allows us to selectively disable some
// phases.
class OperationForTest : public Operation {
 public:
  OperationForTest(base::WeakPtr<OperationManager> manager,
                   const ExtensionId& extension_id,
                   const std::string& storage_unit_id)
      : Operation(manager, extension_id, storage_unit_id) {}

  virtual void Start() OVERRIDE {
  }

  void UnzipStart(scoped_ptr<base::FilePath> zip_file) {
    Operation::UnzipStart(zip_file.Pass());
  }

  void WriteStart() {
    Operation::WriteStart();
  }

  void VerifyWriteStart() {
    Operation::VerifyWriteStart();
  }

  void Finish() {
    Operation::Finish();
  }
 private:
  virtual ~OperationForTest() {};
};

class ImageWriterOperationTest : public ImageWriterUnitTestBase {
 protected:
  virtual void SetUp() OVERRIDE {
    ImageWriterUnitTestBase::SetUp();

    // Create the zip file.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir_.path(),
                                               &image_file_));
    ASSERT_TRUE(base::CreateTemporaryFile(&zip_file_));

    scoped_ptr<char[]> buffer(new char[kTestFileSize]);
    memset(buffer.get(), kImagePattern, kTestFileSize);
    file_util::WriteFile(image_file_, buffer.get(), kTestFileSize);

    zip::Zip(temp_dir_.path(), zip_file_, true);
  }

  virtual void TearDown() OVERRIDE {
    ImageWriterUnitTestBase::TearDown();
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath image_file_;
  base::FilePath zip_file_;
};

} // namespace

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
// Tests a successful unzip.
TEST_F(ImageWriterOperationTest, Unzip) {
  MockOperationManager manager;

  scoped_refptr<OperationForTest> operation(
      new OperationForTest(manager.AsWeakPtr(),
                           kDummyExtensionId,
                           test_device_path_.AsUTF8Unsafe()));

  scoped_ptr<base::FilePath> zip_file(new base::FilePath(zip_file_));

  // At least one progress report > 0% and < 100%.
  EXPECT_CALL(manager, OnProgress(kDummyExtensionId,
                                  image_writer_api::STAGE_UNZIP,
                                  Lt(100))).Times(AtLeast(1));
  // At least one progress report at 100%.
  EXPECT_CALL(manager, OnProgress(kDummyExtensionId,
                                  image_writer_api::STAGE_UNZIP,
                                  100)).Times(AtLeast(1));
  // At least one progress report at 0%.
  EXPECT_CALL(manager, OnProgress(kDummyExtensionId,
                                  image_writer_api::STAGE_UNZIP,
                                  0)).Times(AtLeast(1));
  // Any number of additional progress calls in later stages.
  EXPECT_CALL(manager, OnProgress(kDummyExtensionId,
                                  Gt(image_writer_api::STAGE_UNZIP),
                                  _)).Times(AnyNumber());
  // One completion call.
  EXPECT_CALL(manager, OnComplete(kDummyExtensionId)).Times(1);
  // No errors
  EXPECT_CALL(manager, OnError(_, _, _, _)).Times(0);

  content::BrowserThread::PostTask(content::BrowserThread::FILE,
                                   FROM_HERE,
                                   base::Bind(&OperationForTest::UnzipStart,
                                              operation,
                                              base::Passed(&zip_file)));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(base::ContentsEqual(image_file_, test_device_path_));
}
#endif

TEST_F(ImageWriterOperationTest, Creation) {
  MockOperationManager manager;
  scoped_refptr<Operation> op(
      new OperationForTest(manager.AsWeakPtr(),
                           kDummyExtensionId,
                           test_device_path_.AsUTF8Unsafe()));

  EXPECT_EQ(0, op->GetProgress());
  EXPECT_EQ(image_writer_api::STAGE_UNKNOWN, op->GetStage());
}

}  // namespace image_writer
}  // namespace extensions
